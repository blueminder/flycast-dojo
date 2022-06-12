#pragma once

#include <mutex>
#include "rend/gui.h"
#include "cfg/cfg.h"
#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "rend/gles/gles.h"
#include "rend/gui_util.h"
#include "version.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "log/LogManager.h"
#include "game_scanner.h"

#include "dojo/DojoGui.hpp"
#include "dojo/DojoFile.hpp"

class GuiSettings
{
public:
    void settings_body_general(ImVec2 normal_padding);
    void settings_body_audio(ImVec2 normal_padding);
    void settings_body_advanced(ImVec2 normal_padding);
    void settings_body_about(ImVec2 normal_padding);
    void settings_body_video(ImVec2 normal_padding);
};