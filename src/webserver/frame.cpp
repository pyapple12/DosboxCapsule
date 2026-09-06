// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver/frame.h"

#include <string>
#include <vector>

#include "gui/capsule_frame.h"
#include "misc/logging.h"

using httplib::Request, httplib::Response;

namespace Webserver {

void GetFrame(const Request&, Response& res)
{
	std::vector<uint8_t> pixels;
	CapsuleFrame::FrameMeta meta;

	if (!CapsuleFrame::Snapshot(pixels, meta)) {
		res.status = httplib::StatusCode::ServiceUnavailable_503;
		res.set_content("No frame available yet", "text/plain");
		return;
	}

	// 元数据走响应头，body 保持原始像素二进制（避免 base64 膨胀 1/3 带宽，
	// 30-60fps 轮询下体积敏感）
	res.set_header("X-Capsule-Frame-Number", std::to_string(meta.number));
	res.set_header("X-Capsule-Frame-Width", std::to_string(meta.width));
	res.set_header("X-Capsule-Frame-Height", std::to_string(meta.height));
	res.set_header("X-Capsule-Frame-Pitch", std::to_string(meta.pitch_bytes));

	res.set_content(reinterpret_cast<const char*>(pixels.data()),
	                pixels.size(),
	                "application/octet-stream");
}

} // namespace Webserver
