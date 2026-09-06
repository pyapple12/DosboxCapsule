// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/capsule_frame.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace CapsuleFrame {

namespace {
std::mutex mtx                       = {};
std::vector<uint8_t> buffer          = {}; // 最新帧（紧凑 BGRX，width*4 每行）
FrameMeta meta                       = {};
std::atomic<bool> enabled{false};
} // namespace

void SetEnabled(const bool on)
{
	enabled.store(on, std::memory_order_relaxed);
}

void Capture(const uint8_t* pixels, const int pitch_bytes, const int width,
             const int height)
{
	// 快路径：未启用（webserver 关闭）或参数非法时直接返回，
	// 渲染线程在此处只花一次原子读
	if (!enabled.load(std::memory_order_relaxed) || !pixels || width <= 0 ||
	    height <= 0 || pitch_bytes < width * 4) {
		return;
	}

	const auto row_bytes = static_cast<size_t>(width) * 4;

	std::lock_guard<std::mutex> lock(mtx);
	buffer.resize(row_bytes * static_cast<size_t>(height));
	for (int y = 0; y < height; ++y) {
		std::memcpy(buffer.data() + row_bytes * static_cast<size_t>(y),
		            pixels + static_cast<size_t>(y) * static_cast<size_t>(pitch_bytes),
		            row_bytes);
	}
	meta.width       = static_cast<uint32_t>(width);
	meta.height      = static_cast<uint32_t>(height);
	meta.pitch_bytes = static_cast<uint32_t>(row_bytes);
	++meta.number;
}

bool Snapshot(std::vector<uint8_t>& out_pixels, FrameMeta& out_meta)
{
	std::lock_guard<std::mutex> lock(mtx);
	if (meta.number == 0) {
		return false;
	}
	out_pixels = buffer;
	out_meta   = meta;
	return true;
}

} // namespace CapsuleFrame
