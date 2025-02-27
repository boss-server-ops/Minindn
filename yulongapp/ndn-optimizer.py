#!/usr/bin/env python3
import asyncio
import json
import time
from typing import Dict, Optional
from ndn.app import NDNApp
from ndn.encoding import Name, FormalName, NonStrictName

app = NDNApp()

forwarder_reports: Dict[str, dict] = {}
current_version = 0
config_store: Dict[str, dict] = {}

async def subscribe_reports(name: FormalName, param, app_param: Optional[bytes]):
    if app_param is None:
        print("收到无 ApplicationParameters 的 Interest，丢弃")
        return
    try:
        if isinstance(app_param, memoryview):
            app_param = app_param.tobytes()
        report_json = json.loads(app_param.decode())
        forwarder_id = report_json["forwarder_id"]
        forwarder_reports[forwarder_id] = report_json
        print(f"收到来自 {forwarder_id} 的上报消息: {report_json}")
        confirmation = "Report received".encode()
        app.put_data(name, content=confirmation, freshness_period=500)
    except Exception as e:
        print(f"解析上报消息失败: {e}")

async def periodic_optimization():
    global current_version
    while True:
        await asyncio.sleep(4)
        if not forwarder_reports:
            print("无上报消息，跳过优化")
            continue
        config = {}
        for forwarder_id, report in forwarder_reports.items():
            for client_request in report["client_requests"]:
                client_id = client_request["client_id"]
                acceptable_resolutions = client_request["acceptable_resolutions"]
                recommended_resolution = acceptable_resolutions[len(acceptable_resolutions) // 2]
                config[client_id] = recommended_resolution
        current_version += 1
        config_object = {"version": str(current_version), "config": config}
        config_store[str(current_version)] = config_object
        print(f"生成新配置，版本: {current_version}, 内容: {config_object}")
        forwarder_reports.clear()

async def respond_to_config_query(name: FormalName, param, app_param: Optional[bytes]):
    try:
        name_str = Name.to_str(name)
        print(f"收到配置查询请求: {name_str}")
        name_components = Name.to_str(name).split('/')
        if len(name_components) < 5 or name_components[-2] != "version":
            raise ValueError("无效的配置查询名称格式，期望 /ndn/opt/config/version/X")
        query_version = name_components[-1]
        latest_version = str(current_version)
        print(f"查询版本: {query_version}, 当前最新版本: {latest_version}")

        # 如果查询的版本小于当前版本，返回 NACK
        if int(query_version) < int(latest_version):
            nack_message = {
                "Reason": "Version Outdated",
                "Latest Version": latest_version,
                "Timestamp": time.time()
            }
            nack_payload = json.dumps(nack_message).encode()
            app.put_data(name, content=nack_payload, freshness_period=500)
            print(f"返回 NACK，最新版本: {latest_version}")
        # 如果查询的是当前版本，检查是否有配置
        elif int(query_version) == int(latest_version):
            if latest_version in config_store:
                config_object = config_store[latest_version]
                config_payload = json.dumps(config_object).encode()
                app.put_data(name, content=config_payload, freshness_period=500)
                print(f"返回最新配置，版本: {latest_version}")
            else:
                # 如果配置尚未生成，等待一小段时间后重试
                await asyncio.sleep(1)  # 等待优化完成
                if latest_version in config_store:
                    config_object = config_store[latest_version]
                    config_payload = json.dumps(config_object).encode()
                    app.put_data(name, content=config_payload, freshness_period=500)
                    print(f"返回最新配置，版本: {latest_version} (等待后)")
                else:
                    nack_message = {
                        "Reason": "Config Not Ready",
                        "Latest Version": latest_version,
                        "Timestamp": time.time()
                    }
                    nack_payload = json.dumps(nack_message).encode()
                    app.put_data(name, content=nack_payload, freshness_period=500)
                    print(f"返回 NACK，配置未就绪，最新版本: {latest_version}")
        else:
            # 如果查询的版本大于当前版本，返回错误
            nack_message = {
                "Reason": "Version Too High",
                "Latest Version": latest_version,
                "Timestamp": time.time()
            }
            nack_payload = json.dumps(nack_message).encode()
            app.put_data(name, content=nack_payload, freshness_period=500)
            print(f"返回 NACK，查询版本过高，最新版本: {latest_version}")
    except Exception as e:
        print(f"处理配置查询失败: {e}")

def wrap_respond_to_config_query(name: FormalName, param, app_param: Optional[bytes]):
    print(f"尝试处理配置查询: {Name.to_str(name)}")
    asyncio.ensure_future(respond_to_config_query(name, param, app_param))

def wrap_subscribe_reports(name: FormalName, param, app_param: Optional[bytes]):
    print(f"尝试处理上报消息: {Name.to_str(name)}")
    asyncio.ensure_future(subscribe_reports(name, param, app_param))

async def main():
    print("开始注册路由")
    app.route("/ndn/opt/report")(wrap_subscribe_reports)
    app.route("/ndn/opt/config/version")(wrap_respond_to_config_query)
    print("优化节点路由已注册: /ndn/opt/report 和 /ndn/opt/config/version")
    asyncio.create_task(periodic_optimization())

if __name__ == '__main__':
    try:
        app.run_forever(after_start=main())
    except KeyboardInterrupt:
        print("退出优化节点")
