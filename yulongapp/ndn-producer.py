#!/usr/bin/env python3
import asyncio
import logging
from ndn.app import NDNApp
from ndn.encoding import Name, Component

logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

VIDEO_STORAGE = {
    "TitleA": {
        "2K": {1: "VideoData_2K_chunk_1", 2: "VideoData_2K_chunk_2"},
        "4K": {1: "VideoData_4K_chunk_1", 2: "VideoData_4K_chunk_2"},
        "8K": {1: "VideoData_8K_chunk_1", 2: "VideoData_8K_chunk_2"}
    }
}

app = NDNApp()

async def lookup_video_content(title_id: str, resolution: str, chunk: int) -> str | None:
    try:
        return VIDEO_STORAGE[title_id][resolution][chunk]
    except KeyError:
        return None

async def handle_interest(name, app_param, context):
    name_str = Name.to_str(name)
    name_parts = name_str.split('/')
    
    if len(name_parts) != 8 or name_parts[6] != "chunk":
        logger.debug(f"忽略非标准 Interest 请求: {name_str}")
        return

    try:
        title_id = name_parts[4]
        resolution = name_parts[5]
        chunk = int(name_parts[7])
    except (IndexError, ValueError) as e:
        logger.warning(f"解析 Interest 名称失败: {name_str}, 错误: {e}")
        return

    logger.info(f"收到 Interest: {name_str}")
    video_data = await lookup_video_content(title_id, resolution, chunk)
    
    signer = app.keychain.get_signer({})
    
    if video_data is None:
        logger.warning(f"未找到视频数据: {title_id}/{resolution}/chunk/{chunk}")
        content = "Content not found".encode()
        logger.info(f"发送 Data 数据包: {name_str}")
        app.put_data(name, content=content, signer=signer, freshness_period=10000)
        return

    content = video_data.encode()
    logger.info(f"发送 Data 数据包: {name_str}")
    app.put_data(name, content=content, signer=signer, freshness_period=10000)

def on_interest(name, app_param, context):
    asyncio.ensure_future(handle_interest(name, app_param, context))

async def main():
    try:
        logger.debug("尝试连接 NFD 并注册前缀 /ndn/video/content...")
        await app.register('/ndn/video/content', on_interest)
        logger.info("生产者已启动，正在监听 /ndn/video/content 上的 Interest 请求...")
    except Exception as e:
        logger.error(f"启动生产者失败: {type(e).__name__}: {e}")
        raise

if __name__ == '__main__':
    try:
        app.run_forever(after_start=main())
    except KeyboardInterrupt:
        logger.info("生产者关闭")
    except Exception as e:
        logger.error(f"生产者异常终止: {type(e).__name__}: {e}")
