// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_CAPSULE_FRAME_H
#define DOSBOX_GUI_CAPSULE_FRAME_H

#include <cstdint>
#include <vector>

// Capsule 帧流：把最新完成的 DOS 帧暴露给 webserver 线程（GET /api/v2/frame），
// 供 launcher 画布成像（统一窗口主线，见 CapsuleRetro tools/dosbox-fork/PATCHES.md）。
//
// 设计要点：
// - 渲染路径每帧结束时无条件调用 Capture()，未启用时只花一次原子读短路——
//   webserver 关闭时对正常游戏零负担；
// - 启用后单 mutex 保护最新帧副本，帧号随每次捕获单调递增（内容未变的帧
//   不经 EndFrame，天然不会虚增帧号）；
// - 快照按行紧凑化为 width*4 字节（源行距可能含对齐填充），读方拿到
//   pitch_bytes 即可线性解码 BGRX 像素。
namespace CapsuleFrame {

struct FrameMeta {
	uint64_t number      = 0; // 帧号（自 webserver 启用起递增，0 = 尚无帧）
	uint32_t width       = 0; // 帧宽（像素）
	uint32_t height      = 0; // 帧高（像素）
	uint32_t pitch_bytes = 0; // 快照每行字节数（紧凑 = width * 4）
};

// webserver 启停时调用。未启用时 Capture() 为空操作。
void SetEnabled(bool enabled);

// 渲染线程调用：捕获一帧 DOS 内容（32bit BGRX，源行距 pitch_bytes，可能含对齐）。
// 内部按行紧凑拷贝并递增帧号；参数非法时静默丢弃本帧。
void Capture(const uint8_t* pixels, int pitch_bytes, int width, int height);

// webserver 线程调用：取当前帧快照写入 out_pixels（紧凑 BGRX）。
// 尚无帧可用时返回 false。
bool Snapshot(std::vector<uint8_t>& out_pixels, FrameMeta& out_meta);

} // namespace CapsuleFrame

#endif // DOSBOX_GUI_CAPSULE_FRAME_H
