// SPDX-FileCopyrightText:  2026 CapsuleRetro project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver/input.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "gui/mapper.h"
#include "hardware/input/mouse.h"
#include "libs/json/json.h"
#include "webserver/bridge.h"
#include "webserver/webserver.h"

#include <SDL.h>

using json = nlohmann::json;
using httplib::Request, httplib::Response;

namespace Webserver {

namespace {

// 键名 -> SDL scancode（统一窗口画布的转发协议，覆盖 DOS 游戏常用键）
const std::unordered_map<std::string, SDL_Scancode>& key_lookup()
{
	static const std::unordered_map<std::string, SDL_Scancode> lookup = {
	        {"return", SDL_SCANCODE_RETURN},      {"escape", SDL_SCANCODE_ESCAPE},
	        {"space", SDL_SCANCODE_SPACE},        {"tab", SDL_SCANCODE_TAB},
	        {"backspace", SDL_SCANCODE_BACKSPACE},{"up", SDL_SCANCODE_UP},
	        {"down", SDL_SCANCODE_DOWN},          {"left", SDL_SCANCODE_LEFT},
	        {"right", SDL_SCANCODE_RIGHT},        {"f1", SDL_SCANCODE_F1},
	        {"f2", SDL_SCANCODE_F2},              {"f3", SDL_SCANCODE_F3},
	        {"f4", SDL_SCANCODE_F4},              {"f5", SDL_SCANCODE_F5},
	        {"f6", SDL_SCANCODE_F6},              {"f7", SDL_SCANCODE_F7},
	        {"f8", SDL_SCANCODE_F8},              {"f9", SDL_SCANCODE_F9},
	        {"f10", SDL_SCANCODE_F10},            {"f11", SDL_SCANCODE_F11},
	        {"f12", SDL_SCANCODE_F12},            {"a", SDL_SCANCODE_A},
	        {"b", SDL_SCANCODE_B},                {"c", SDL_SCANCODE_C},
	        {"d", SDL_SCANCODE_D},                {"e", SDL_SCANCODE_E},
	        {"f", SDL_SCANCODE_F},                {"g", SDL_SCANCODE_G},
	        {"h", SDL_SCANCODE_H},                {"i", SDL_SCANCODE_I},
	        {"j", SDL_SCANCODE_J},                {"k", SDL_SCANCODE_K},
	        {"l", SDL_SCANCODE_L},                {"m", SDL_SCANCODE_M},
	        {"n", SDL_SCANCODE_N},                {"o", SDL_SCANCODE_O},
	        {"p", SDL_SCANCODE_P},                {"q", SDL_SCANCODE_Q},
	        {"r", SDL_SCANCODE_R},                {"s", SDL_SCANCODE_S},
	        {"t", SDL_SCANCODE_T},                {"u", SDL_SCANCODE_U},
	        {"v", SDL_SCANCODE_V},                {"w", SDL_SCANCODE_W},
	        {"x", SDL_SCANCODE_X},                {"y", SDL_SCANCODE_Y},
	        {"z", SDL_SCANCODE_Z},                {"1", SDL_SCANCODE_1},
	        {"2", SDL_SCANCODE_2},                {"3", SDL_SCANCODE_3},
	        {"4", SDL_SCANCODE_4},                {"5", SDL_SCANCODE_5},
	        {"6", SDL_SCANCODE_6},                {"7", SDL_SCANCODE_7},
	        {"8", SDL_SCANCODE_8},                {"9", SDL_SCANCODE_9},
	        {"0", SDL_SCANCODE_0},
	        // 修饰键本身也是 mapper 绑定键（如 lalt = 全屏组合键的成员），
	        // 客户端须按时序注入：修饰键 down -> 主键 down/up -> 修饰键 up
	        {"leftalt", SDL_SCANCODE_LALT},       {"rightalt", SDL_SCANCODE_RALT},
	        {"leftctrl", SDL_SCANCODE_LCTRL},     {"rightctrl", SDL_SCANCODE_RCTRL},
	        {"leftshift", SDL_SCANCODE_LSHIFT},   {"rightshift", SDL_SCANCODE_RSHIFT},
	};
	return lookup;
}

// 修饰键名 -> SDL_Keymod
uint16_t lookup_mod(const std::string& name)
{
	static const std::unordered_map<std::string, uint16_t> lookup = {
	        {"alt", KMOD_LALT},   {"leftalt", KMOD_LALT},
	        {"rightalt", KMOD_RALT}, {"ctrl", KMOD_LCTRL},
	        {"leftctrl", KMOD_LCTRL}, {"rightctrl", KMOD_RCTRL},
	        {"shift", KMOD_LSHIFT},  {"leftshift", KMOD_LSHIFT},
	        {"rightshift", KMOD_RSHIFT},
	};
	const auto it = lookup.find(name);
	return it != lookup.end() ? it->second : KMOD_NONE;
}

// Bridge 命令：在模拟线程执行 mapper 分发（mapper 状态无锁，依赖 Bridge
// 保证与真实事件泵同线程；SDL 队列在暂停态会被清空，直调绕开该坑）
struct KeyInjectCommand : public Command {
	SDL_Event event = {};

	void Execute() override { MAPPER_CheckEvent(&event); }
};

bool inject_key(const json& body)
{
	const auto key_name = body.at("key").get<std::string>();
	const auto& lookup  = key_lookup();
	const auto key_it   = lookup.find(key_name);
	if (key_it == lookup.end()) {
		throw std::invalid_argument("Unknown key '" + key_name + "'");
	}

	uint16_t mods = KMOD_NONE;
	if (body.contains("mods")) {
		for (const auto& m : body.at("mods")) {
			mods |= lookup_mod(m.get<std::string>());
		}
	}
	const auto down = body.at("down").get<bool>();

	KeyInjectCommand cmd;
	cmd.event.type             = down ? SDL_KEYDOWN : SDL_KEYUP;
	cmd.event.key.keysym.scancode = key_it->second;
	cmd.event.key.keysym.sym   = SDL_GetKeyFromScancode(key_it->second);
	cmd.event.key.keysym.mod   = mods;
	cmd.event.key.state        = down ? SDL_PRESSED : SDL_RELEASED;
	cmd.event.key.repeat       = 0;
	cmd.WaitForCompletion();

	if (!cmd.error.empty()) {
		throw std::runtime_error(cmd.error);
	}
	return true;
}

// 鼠标注入走 Bridge 直调 MOUSE_InjectMoved/InjectButton（注入原语绕过宿主
// 光标判定——离屏窗口下 cursor_is_outside 恒真，SDL_PushEvent 路径会被丢弃）；
// 与键盘一致在模拟线程执行
struct MouseInjectCommand : public Command {
	bool is_button = false;
	float dx       = 0.0f;
	float dy       = 0.0f;
	MouseButtonId button_id = MouseButtonId::Left;
	bool pressed   = false;

	void Execute() override
	{
		if (is_button) {
			MOUSE_InjectButton(button_id, pressed);
		} else {
			MOUSE_InjectMoved(dx, dy);
		}
	}
};

bool inject_mouse_rel(const json& body)
{
	MouseInjectCommand cmd;
	cmd.dx = static_cast<float>(body.at("dx").get<int32_t>());
	cmd.dy = static_cast<float>(body.at("dy").get<int32_t>());
	cmd.WaitForCompletion();
	if (!cmd.error.empty()) {
		throw std::runtime_error(cmd.error);
	}
	return true;
}

bool inject_mouse_button(const json& body)
{
	const auto button_name = body.at("button").get<std::string>();
	MouseButtonId cmd_button_id = MouseButtonId::Left;
	if (button_name == "left") {
		cmd_button_id = MouseButtonId::Left;
	} else if (button_name == "right") {
		cmd_button_id = MouseButtonId::Right;
	} else if (button_name == "middle") {
		cmd_button_id = MouseButtonId::Middle;
	} else {
		throw std::invalid_argument("Unknown button '" + button_name + "'");
	}

	MouseInjectCommand cmd;
	cmd.is_button = true;
	cmd.button_id = cmd_button_id;
	cmd.pressed   = body.at("down").get<bool>();
	cmd.WaitForCompletion();
	if (!cmd.error.empty()) {
		throw std::runtime_error(cmd.error);
	}
	return true;
}

} // namespace

void PostInputEvent(const Request& req, Response& res)
{
	const auto body = json::parse(req.body);

	if (!body.contains("kind")) {
		throw std::invalid_argument("Missing 'kind' field");
	}
	const auto kind = body.at("kind").get<std::string>();

	bool accepted = false;
	if (kind == "key") {
		accepted = inject_key(body);
	} else if (kind == "mouse") {
		accepted = inject_mouse_rel(body);
	} else if (kind == "click") {
		accepted = inject_mouse_button(body);
	} else {
		throw std::invalid_argument("Unknown kind '" + kind + "'");
	}

	json j;
	j["accepted"] = accepted;
	send_json(res, j);
}

} // namespace Webserver
