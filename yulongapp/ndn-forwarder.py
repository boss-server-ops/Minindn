#!/usr/bin/env python3
import asyncio
import json
import time
from typing import Dict, Optional
from ndn.app import NDNApp
from ndn.encoding import Name, FormalName, NonStrictName

app = NDNApp()

current_version = 0
pending_requests: Dict[str, dict] = {}
last_reported_requests: Dict[str, dict] = {}

async def send_nack_to_client(interest_name: FormalName, nack_payload: bytes):
    try:
        app.put_data(interest_name, content=nack_payload, freshness_period=500)
        print(f"已向客户端发送 NACK: {Name.to_str(interest_name)}")
    except Exception as e:
        print(f"发送 NACK 失败: {e}")

async def forward_standard_interest(standard_interest_name: NonStrictName):
    producer_interest_name = f"/ndn/video/content{standard_interest_name[len('/ndn/video'):]}"
    print(f"转发标准 Interest: {producer_interest_name}")
    try:
        data_name, meta_info, content = await app.express_interest(
            producer_interest_name,
            lifetime=4000
        )
        if content:
            content_str = bytes(content).decode()
            print(f"收到内容服务器响应: {content_str}")
            client_id = list(pending_requests.keys())[0]
            app.put_data(
                pending_requests[client_id]["interest_name"],
                content=content,
                freshness_period=1000
            )
        else:
            print(f"未收到内容服务器响应: {producer_interest_name}")
    except Exception as e:
        print(f"转发标准 Interest 失败: {e}")

async def receive_range_interest(name: FormalName, param, app_param: Optional[bytes]):
    if app_param is None:
        print("收到无 ApplicationParameters 的 Interest，丢弃")
        return
    try:
        payload = json.loads(bytes(app_param).decode())
        client_id = payload["client_id"]
        acceptable_resolutions = payload["acceptable_resolutions"]
        name_str = Name.to_str(name)
        title_id = name_str[len("/ndn/video/"):].split("/RangeInterest")[0]
        components = name_str.split("/")
        chunk_index = components.index("chunk") + 1
        chunk = int(components[chunk_index])
        pending_requests[client_id] = {
            "interest_name": name,
            "title_id": title_id,
            "chunk": chunk,
            "acceptable_resolutions": acceptable_resolutions,
            "timestamp": payload["timestamp"]
        }
        if client_id not in last_reported_requests or last_reported_requests[client_id] != pending_requests[client_id]:
            last_reported_requests[client_id] = {
                "title_id": title_id,
                "chunk": chunk,
                "acceptable_resolutions": acceptable_resolutions,
                "timestamp": payload["timestamp"]
            }
        print(f"收到客户端 {client_id} 的 Range-based Interest: {name_str}")
    except Exception as e:
        print(f"解析 Range-based Interest 失败: {e}")

async def report_to_optimizer():
    reported_requests: Dict[str, dict] = {}
    while True:
        await asyncio.sleep(0.3)
        if not last_reported_requests or last_reported_requests == reported_requests:
            continue
        upstream_link = {"link_name": "Core_Forwarder_1->Edge_Forwarder_A", "remaining_capacity": 120}
        report = {
            "forwarder_id": "Edge_Forwarder_A",
            "client_requests": [
                {
                    "client_id": client_id,
                    "acceptable_resolutions": info["acceptable_resolutions"],
                    "TitleID": info["title_id"],
                    "chunk": info["chunk"],
                    "timestamp": info["timestamp"]
                }
                for client_id, info in last_reported_requests.items()
            ],
            "local_network_state": {"upstream_link": upstream_link}
        }
        report_payload = json.dumps(report).encode()
        try:
            data_name, meta_info, content = await app.express_interest(
                "/ndn/opt/report",
                app_param=report_payload,
                must_be_fresh=True,
                lifetime=4000
            )
            print(f"发送上报消息到优化节点: /ndn/opt/report")
            print(f"Payload: {report}")
            if content:
                print(f"收到优化节点确认: {bytes(content).decode()}")
            reported_requests = last_reported_requests.copy()
            last_reported_requests.clear()
        except Exception as e:
            print(f"发送上报消息失败: {e}")

async def fetch_configuration():
    global current_version
    while True:
        if not pending_requests:
            print("无待处理请求，跳过配置查询")
            await asyncio.sleep(0.3)
            continue
        print(f"查询配置: /ndn/opt/config/version/{current_version}")
        try:
            data_name, meta_info, content = await app.express_interest(
                f"/ndn/opt/config/version/{current_version}",
                must_be_fresh=True,
                lifetime=4000
            )
            if content:
                print("收到配置响应，准备处理")
                await handle_configuration_response(content)
        except Exception as e:
            print(f"查询配置失败: {e}")
        await asyncio.sleep(0.3)

async def handle_configuration_response(content: bytes):
    global current_version
    try:
        if isinstance(content, memoryview):
            content = content.tobytes()
        config_json = json.loads(content.decode())
        if "version" in config_json and "config" in config_json:
            version = config_json["version"]
            config = config_json["config"]
            print(f"收到配置，版本: {version}, 内容: {config}")
            current_version = int(version)
            for client_id, info in list(pending_requests.items()):
                recommended_resolution = config.get(client_id)
                if not recommended_resolution:
                    print(f"客户端 {client_id} 未在配置中找到推荐分辨率")
                    continue
                nack_message = {
                    "Reason": "Resolution Selection",
                    "Recommended Name": f"/ndn/video/{info['title_id']}/{recommended_resolution}/chunk/{info['chunk']}",
                    "Timestamp": time.time()
                }
                nack_payload = json.dumps(nack_message).encode()
                await send_nack_to_client(info["interest_name"], nack_payload)
                standard_interest_name = nack_message["Recommended Name"]
                await forward_standard_interest(standard_interest_name)
                del pending_requests[client_id]
        elif "Reason" in config_json and "Latest Version" in config_json:
            latest_version = config_json["Latest Version"]
            print(f"收到 NACK，原因是: {config_json['Reason']}，最新版本: {latest_version}")
            if config_json["Reason"] in ["Version Outdated", "Config Not Ready"]:
                current_version = int(latest_version)
        else:
            print(f"收到未知格式的响应: {config_json}")
    except Exception as e:
        print(f"处理配置响应失败: {e}")

def wrap_receive_range_interest(name: FormalName, param, app_param: Optional[bytes]):
    asyncio.ensure_future(receive_range_interest(name, param, app_param))

async def main():
    print("Forwarder 开始运行")
    print("启动 main 函数")
    app.route("/ndn/video")(wrap_receive_range_interest)
    asyncio.create_task(report_to_optimizer())
    asyncio.create_task(fetch_configuration())
    print("所有任务已创建")

if __name__ == "__main__":
    try:
        app.run_forever(after_start=main())
    except KeyboardInterrupt:
        print("Forwarder 关闭")
