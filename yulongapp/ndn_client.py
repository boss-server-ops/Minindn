#!/usr/bin/env python3
import asyncio
import json
import time
from ndn.app import NDNApp

app = NDNApp()  # 使用默认Keychain连接NFD

async def build_range_interest(title_id: str, chunk: int, client_id: str, acceptable_resolutions: list, priority: int):
    """
    构造 Range-based Interest 的名称与 JSON 格式 payload
    名称格式：/ndn/video/<TitleID>/RangeInterest/chunk=<seq>
    """
    interest_name = f'/ndn/video/{title_id}/RangeInterest/chunk/{chunk}'
    payload_dict = {
        "client_id": client_id,
        "acceptable_resolutions": acceptable_resolutions,
        "priority": priority,
        "timestamp": time.time()
    }
    payload = json.dumps(payload_dict).encode()  # 转为字节串
    return interest_name, payload

async def send_range_interest(interest_name: str, payload: bytes):
    """
    通过 express_interest 发送带有 ApplicationParameters 的 Range-based Interest
    返回的 Data 包即为经过签名的 NACK 消息
    """
    print(f"发送 Range-based Interest: {interest_name}\nPayload: {bytes(payload).decode()}")
    try:
        # interest中携带了app_param，故会被签名（默认使用app内置的 Keychain）
        data_name, meta_info, content = await app.express_interest(
            interest_name,
            app_param=payload,
            lifetime=40000  # Interest超时时间，单位毫秒
        )
        return data_name, meta_info, content
    except Exception as e:
        print("发送 Range-based Interest 失败:", {e})
        return None, None, None

async def build_standard_interest(title_id: str, recommended_resolution: str, chunk: int):
    """
    构造标准 Interest 的名称
    格式：/ndn/video/content/<TitleID>/<推荐分辨率>/
    """
    standard_interest_name = f'/ndn/video/content/{title_id}/{recommended_resolution}/chunk/{chunk}'
    return standard_interest_name

async def send_standard_interest(interest_name: str):
    """
    发送标准 Interest 请求获取视频数据
    """
    print(f"发送标准 Interest: {interest_name}")
    try:
        data_name, meta_info, content = await app.express_interest(
            interest_name,
            lifetime=4000
        )
        return data_name, meta_info, content
    except Exception as e:
        print("发送标准 Interest 失败:", e)
        return None, None, None

async def main():
    # 配置客户端参数
    title_id = "TitleA"
    chunk = 1
    client_id = "A"
    acceptable_resolutions = ["2K", "4K", "8K"]
    priority = 1

    # 1. 构造并发送 Range-based Interest
    range_interest_name, payload = await build_range_interest(title_id, chunk, client_id, acceptable_resolutions, priority)
    nack_data_name, meta_info, nack_content = await send_range_interest(range_interest_name, payload)
    
    if nack_content is None:
        print("未收到 NACK 消息，退出。")
        return

    print(f"收到 NACK 数据包，名称: {nack_data_name}")
    try:
        # 假设 NACK 的内容为 JSON 格式，如：
        # {
        #   "Reason": "Resolution Selection",
        #   "Recommended Name": "/ndn/video/TitleA/4K/chunk/1",
        #   "Timestamp": "..."
        # }
        nack_json = json.loads(bytes(nack_content).decode())
        recommended_name = nack_json.get("Recommended Name", "")
        if not recommended_name:
            print("NACK 中未包含推荐名称信息。")
            return
        # 解析推荐名称中的分辨率
        # 例如推荐名称格式为：/ndn/video/TitleA/4K/chunk/1
        name_parts = recommended_name.split('/')
        if len(name_parts) >= 5:
            # name_parts[0]为空，name_parts[1]="ndn", [2]="video", [3]=TitleID, [4]=推荐分辨率
            recommended_resolution = name_parts[4]
        else:
            print("推荐名称格式错误。")
            return
    except Exception as e:
        print("解析 NACK 内容失败:", e)
        return

    print(f"解析到推荐分辨率: {recommended_resolution}")

    # 2. 构造并发送标准 Interest 获取视频数据
    standard_interest_name = await build_standard_interest(title_id, recommended_resolution, chunk)
    data_name, meta_info, content = await send_standard_interest(standard_interest_name)
    if content is not None:
        print(f"收到视频数据, 名称: {data_name}\n内容: {bytes(content).decode()}")
    else:
        print("未收到视频数据。")

if __name__ == '__main__':
    try:
        app.run_forever(after_start=main())
    except KeyboardInterrupt:
        print("退出客户端")
