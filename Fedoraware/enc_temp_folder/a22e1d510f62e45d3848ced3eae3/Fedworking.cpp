#include "Fedworking.h"
#include "../../Utils/Base64/Base64.hpp"

enum MessageType {
	None,
	Marker	// [ Type, X-Pos, Y-Pos, Z-Pos, Player-ID ]
};

void CFedworking::HandleMessage(const char* pMessage)
{
	std::string encMsg(pMessage);
	encMsg.erase(0, 4); // Remove FED@ prefix

	const std::string msg = Base64::Decode(encMsg);
	if (msg.empty()) { return; }

	const auto dataVector = Utils::SplitString(msg, "&");
	int msgType;
	try { msgType = std::stoi(dataVector[0]); }
	catch (...) { ConsoleLog("Failed to read message type!"); return; }

	switch (msgType) {
	case None:
		{
			// Undefined message type (Sent by party crasher)
			break;
		}

	case Marker:
		{
			if (dataVector.size() == 5) {
				try {
					const float xPos = std::stof(dataVector[1]);
					const float yPos = std::stof(dataVector[2]);
					const float zPos = std::stof(dataVector[3]);
					const int playerIndex = std::stoi(dataVector[4]);

					PlayerInfo_t playerInfo{};
					g_Interfaces.Engine->GetPlayerInfo(playerIndex, &playerInfo);

					if (playerInfo.userID != 0) {
						CGameEvent* markerEvent = g_Interfaces.GameEvent->CreateNewEvent("show_annotation");
						if (markerEvent) {
							markerEvent->SetInt("id", playerIndex);
							markerEvent->SetFloat("worldPosX", xPos);
							markerEvent->SetFloat("worldPosY", yPos);
							markerEvent->SetFloat("worldPosZ", zPos);
							markerEvent->SetFloat("lifetime", 5.0f);

							markerEvent->SetBool("show_distance", true);
							markerEvent->SetString("text", playerInfo.name);
							markerEvent->SetString("play_sound", "coach/coach_go_here.wav");

							g_Interfaces.GameEvent->FireEvent(markerEvent);
						}
					}
				}
				catch (...) { ConsoleLog("Failed to read marker data!"); }
			}
			break;
		}

	default:
		{
			// Unknown message type
			ConsoleLog("Unknown message received!");
			break;
		}
	}
}

void CFedworking::SendMarker(const Vec3& pPos, int pEntityIndex)
{
	const std::string xPos = std::to_string(pPos.x);
	const std::string yPos = std::to_string(pPos.y);
	const std::string zPos = std::to_string(pPos.z);

	std::stringstream msg;
	msg << Marker << "&" << xPos << "&" << yPos << "&" << zPos << "&" << pEntityIndex;
	SendMessage(msg.str());
}

void CFedworking::SendMessage(const std::string& pData)
{
	const std::string encMsg = Base64::Encode(pData);
	if (encMsg.size() <= 253) {
		std::string cmd = "tf_party_chat \"FED@";
		cmd.append(encMsg);
		cmd.append("\"");
		g_Interfaces.Engine->ClientCmd_Unrestricted(cmd.c_str());
	} else {
		ConsoleLog("Failed to send message! The message was too long.");
	}
}

void CFedworking::Run()
{
	if (!Vars::Misc::PartyNetworking.m_Var) { return; }

	if (const auto& pLocal = g_EntityCache.m_pLocal) {
		// Party marker
		if (Vars::Misc::PartyMarker.m_Var && GetAsyncKeyState(Vars::Misc::PartyMarker.m_Var)) {
			const Vec3 viewAngles = g_Interfaces.Engine->GetViewAngles();
			Vec3 vForward;
			Math::AngleVectors(viewAngles, &vForward);

			CGameTrace trace;
			CTraceFilterHitscan traceFilter = {};
			Ray_t traceRay;
			vForward = pLocal->GetEyePosition() + vForward * MAX_TRACE_LENGTH;
			traceRay.Init(pLocal->GetEyePosition(), vForward);
			g_Interfaces.EngineTrace->TraceRay(traceRay, MASK_SOLID, &traceFilter, &trace);
			if (trace.DidHit()) {
				g_Interfaces.DebugOverlay->AddLineOverlay(trace.vStartPos, trace.vEndPos, 255, 0, 0, false, 1.0f);
				g_Fedworking.SendMarker(trace.vEndPos, pLocal->GetIndex());
			}
		}
	}
}

void CFedworking::ConsoleLog(const std::string& pMessage)
{
	std::string consoleMsg = "[Fedworking] ";
	consoleMsg.append(pMessage);
	consoleMsg.append("\n");
	g_Interfaces.CVars->ConsoleColorPrintf({ 225, 177, 44, 255 }, consoleMsg.c_str());
}