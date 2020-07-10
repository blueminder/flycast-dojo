/*
	This file is part of libswirl
*/
#include "license/bsd"
#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "reios/reios_syscalls.h"
#include "reios/reios_dbg.h"
#include <sstream>

#include "hw/sh4/sh4_mem.h"

void gui_settings_advanced()
{
	if (ImGui::BeginTabItem("Advanced"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	    if (ImGui::CollapsingHeader("MCPU Mode", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Checkbox("Debugger", &g_reios_dbg_enabled);
			ImGui::SameLine();
			gui_ShowHelpMarker("Enable / Disable HLE Debugger(Developer only)");
			ImGui::Columns(2, "sh4_modes", false);
			ImGui::RadioButton("MCPU Dynarec", &dynarec_enabled, 1);
            ImGui::SameLine();
            gui_ShowHelpMarker("Use the dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("MCPU Interpreter", &dynarec_enabled, 0);
            ImGui::SameLine();
            gui_ShowHelpMarker("Use the interpreter. Very slow but may help in case of a dynarec problem");
			ImGui::Columns(1, NULL, false);
	    }

#if 0
		const auto dbg_mode = ( g_reios_dbg_enabled )? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

		if (ImGui::CollapsingHeader("HLE Debugger", dbg_mode)) {
			if (g_reios_dbg_enabled) {
				if (ImGui::CollapsingHeader("Registers", dbg_mode)) {
					ImGui::LabelText("","PC=0x%08x", Sh4cntx.pc);
					ImGui::LabelText("", "SR=0x%08x", Sh4cntx.sr);
					ImGui::LabelText("", "oldSR0x%08x", Sh4cntx.old_sr);
					ImGui::LabelText("", "SP=0x%08x", Sh4cntx.spc);
					ImGui::LabelText("", "Last opcode=0x%08x", reios_dbg_get_last_op());

					const size_t frc = sizeof(Sh4cntx.xffr) / sizeof(Sh4cntx.xffr[0]);
					const size_t rc = sizeof(Sh4cntx.r) / sizeof(Sh4cntx.r[0]);
					const size_t gt = (frc > rc) ? frc : rc;


					ImGui::NewLine();
					ImGui::Text("GPRS:");
					ImGui::NewLine();
					for (size_t i = 0; i < rc; ++i) {
						if ((i) && (0 == (i & 7)))
							ImGui::NewLine();
						else
							ImGui::SameLine();

						ImGui::Text("R[%d]=0x%08x", i, Sh4cntx.r[i]);
					}

					ImGui::NewLine();
					ImGui::Text("FPRS:");
					ImGui::NewLine();
					for (size_t i = 0; i < frc; ++i) {
						if ((i) && (0 == (i & 7)))
							ImGui::NewLine();
						else
							ImGui::SameLine();
						ImGui::Text("FR[%d]=%f", i, Sh4cntx.xffr[i]);
					}
					ImGui::NewLine();
				}

				//mem

				if (ImGui::CollapsingHeader("Memory Editor", dbg_mode)) {
					ImGui::Checkbox("Hex", &settings.dynarec.safemode);
					ImGui::SameLine();

					ImGui::Checkbox("char", &settings.dynarec.safemode);
					ImGui::SameLine();


					ImGui::Checkbox("int", &settings.dynarec.safemode);
					ImGui::SameLine();

					ImGui::Checkbox("float", &settings.dynarec.safemode);
					ImGui::NewLine();


					ImGui::LabelText("", "Displaying range 0x%08x~0x%08x", 0x8021c000, 0x8021c000 + 128);
					ImGui::NewLine();

					for (size_t i = 0; i < 128; ++i) {

						std::stringstream t;
						uint8_t ss = ReadMem8(0x8021c000 + i);
						ImGui::Text("%02x", ss); 
						
						
						if ((i)&& (0==(i & 31)))
							ImGui::NewLine();
						else
							ImGui::SameLine();
						
					} 

					ImGui::NewLine();
					
				}

				if (ImGui::CollapsingHeader("Callstack", dbg_mode)) {
				}
			}
		}

#endif
	    if (ImGui::CollapsingHeader("SH4 Dynarec Options", dynarec_enabled ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
	    {
	    	ImGui::Checkbox("Safe Mode", &settings.dynarec.safemode);
            ImGui::SameLine();
            gui_ShowHelpMarker("Do not optimize integer division. Recommended");
	    	ImGui::Checkbox("Unstable Optimizations", &settings.dynarec.unstable_opt);
            ImGui::SameLine();
            gui_ShowHelpMarker("Enable unsafe optimizations. Will cause crash or environmental disaster");
	    	ImGui::Checkbox("Idle Skip", &settings.dynarec.idleskip);
            ImGui::SameLine();
            gui_ShowHelpMarker("Skip wait loops. Recommended");
			ImGui::PushItemWidth(ImGui::CalcTextSize("Largeenough").x);
            const char *preview = settings.dynarec.SmcCheckLevel == NoCheck ? "Faster" : settings.dynarec.SmcCheckLevel == FastCheck ? "Fast" : "Full";
			if (ImGui::BeginCombo("SMC Checks", preview	, ImGuiComboFlags_None))
			{
				bool is_selected = settings.dynarec.SmcCheckLevel == NoCheck;
				if (ImGui::Selectable("Faster", &is_selected))
					settings.dynarec.SmcCheckLevel = NoCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				is_selected = settings.dynarec.SmcCheckLevel == FastCheck;
				if (ImGui::Selectable("Fast", &is_selected))
					settings.dynarec.SmcCheckLevel = FastCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				is_selected = settings.dynarec.SmcCheckLevel == FullCheck;
				if (ImGui::Selectable("Full", &is_selected))
					settings.dynarec.SmcCheckLevel = FullCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            gui_ShowHelpMarker("How to detect self-modifying code. Full check recommended");
	    }
		if (ImGui::CollapsingHeader("SCPU Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Columns(2, "arm7_modes", false);
			ImGui::RadioButton("SCPU Dynarec", &settings.dynarec.ScpuEnable, 1);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the ARM7 dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("SCPU Interpreter", &settings.dynarec.ScpuEnable, 0);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the ARM7 interpreter. Very slow but may help in case of a dynarec problem");
			ImGui::Columns(1, NULL, false);
		}
		if (ImGui::CollapsingHeader("DSP Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Columns(2, "dsp_modes", false);
			ImGui::RadioButton("DSP Dynarec", &settings.dynarec.DspEnable, 1);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the DSP dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("DSP Interpreter", &settings.dynarec.DspEnable, 0);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the DSP interpreter. Very slow but may help in case of a DSP dynarec problem");
			ImGui::Columns(1, NULL, false);
		}
		
		if (ImGui::CollapsingHeader("Cloudroms", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Checkbox("Hide Homebrew", &settings.cloudroms.HideHomebrew);
            ImGui::SameLine();
            gui_ShowHelpMarker("Hide the homebrew category on cloudroms");

			ImGui::Checkbox("Show archive.org", &settings.cloudroms.ShowArchiveOrg);
            ImGui::SameLine();
            gui_ShowHelpMarker("Show the archive.org category on cloudroms. Please check your local laws on whenever this is legal for you.");
		}

		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
	    {
#ifndef _ANDROID
			ImGui::Checkbox("Serial Console", &settings.debug.SerialConsole);
            ImGui::SameLine();
            gui_ShowHelpMarker("Dump the Dreamcast serial console to stdout");
#endif

#if FEAT_HAS_SERIAL_TTY
			ImGui::Checkbox("Create Virtual Serial Port", &settings.debug.VirtualSerialPort);
            ImGui::SameLine();
            gui_ShowHelpMarker("Create a PTY for use with dc-load. Requires restart.");
#endif

			ImGui::Checkbox("Dump Textures", &settings.rend.DumpTextures);
            ImGui::SameLine();
            gui_ShowHelpMarker("Dump all textures into data/texdump/<game id>");

			ImGui::Checkbox("HLE Emulation", &settings.bios.UseReios);
			ImGui::SameLine();
			gui_ShowHelpMarker("Enable / Disable High level emulation(aka no BIOS required)");


			
	    }

		if (ImGui::CollapsingHeader("HLE Stubs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto i : g_syscalls_mgr.get_map()) {
				std::stringstream t;
				t << "(pc:0x" << std::hex << (i.second.addr) << ")";
				t << "(scall:0x" << std::hex << (i.second.syscall) << ")";
				ImGui::Checkbox( std::string(i.first  + t.str() ).c_str(), &i.second.enabled);
				
			}
			
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}
