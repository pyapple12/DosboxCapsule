// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver/frame.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gui/capsule_frame.h"
#include "misc/logging.h"

using httplib::Request, httplib::Response;

namespace {

// 小端序写入（流式帧内头协议字段，全部 LE）
void put_le64(std::vector<uint8_t>& out, const uint64_t v)
{
	for (int i = 0; i < 8; ++i) {
		out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
	}
}

void put_le32(std::vector<uint8_t>& out, const uint32_t v)
{
	for (int i = 0; i < 4; ++i) {
		out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
	}
}

} // namespace

namespace Webserver {

void GetFrameStream(const Request&, Response& res)
{
	// Capsule 流式直推：HTTP chunked 长连接，每捕获一帧即推送——帧在捕获
	// 瞬间朝客户端移动，消灭轮询的 0~33ms 固有陈旧度。客户端断开由
	// sink.is_cancelled() / write 失败感知，provider 返回 false 结束推送。
	// 注：vendored cpp-httplib 不含 WebSocket 支持；chunked 推送与 WS 的
	// 推送语义、延迟特征一致，免去自研握手/封帧协议的风险。
	res.set_header("Access-Control-Allow-Origin", "*");
	res.set_chunked_content_provider(
		"application/octet-stream",
		[](size_t, httplib::DataSink& sink) -> bool {
			uint64_t seen_number = 0;
			std::vector<uint8_t> pixels;
			CapsuleFrame::FrameMeta meta;

			while (true) {
				// 超时醒来例行检查客户端断开（is_writable=false 即连接已关）；
				// 未断开则继续等下一帧
				if (!CapsuleFrame::WaitSnapshot(seen_number, pixels, meta, 250)) {
					if (!sink.is_writable()) {
						return false;
					}
					continue;
				}
				seen_number = meta.number;

				std::vector<uint8_t> packet;
				packet.reserve(28 + pixels.size());
				put_le64(packet, meta.number);
				put_le32(packet, meta.width);
				put_le32(packet, meta.height);
				put_le32(packet, meta.pitch_bytes);
				put_le64(packet, meta.capture_ms);
				packet.insert(packet.end(), pixels.begin(), pixels.end());

				if (!sink.write(reinterpret_cast<const char*>(packet.data()),
				                packet.size())) {
					return false;
				}
			}
		});
}

} // namespace Webserver
