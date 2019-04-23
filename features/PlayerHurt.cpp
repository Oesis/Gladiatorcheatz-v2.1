#include "PlayerHurt.hpp"

#include "../Structs.hpp"
#include "Visuals.hpp"
#include "Resolver.hpp"

#include "../Options.hpp"

#include <algorithm>

void PlayerHurtEvent::FireGameEvent(IGameEvent *event)
{
	if (!g_LocalPlayer || !event)
		return;

	if (g_Options.visuals_others_hitmarker)
	{
		if (g_EngineClient->GetPlayerForUserID(event->GetInt("attacker")) == g_EngineClient->GetLocalPlayer() &&
			g_EngineClient->GetPlayerForUserID(event->GetInt("userid"))   != g_EngineClient->GetLocalPlayer())
		{
			hitMarkerInfo.push_back({ g_GlobalVars->curtime + 0.8f, event->GetInt("dmg_health") });
			g_EngineClient->ExecuteClientCmd("play buttons\\arena_switch_press_02.wav"); // No other fitting sound. Probs should use a resource
		}
	}

	if (g_Options.esp_lagcompensated_hitboxes)
	{
		int32_t attacker = g_EngineClient->GetPlayerForUserID(event->GetInt("attacker"));
		int32_t userid = g_EngineClient->GetPlayerForUserID(event->GetInt("userid"));

		if (attacker == g_EngineClient->GetLocalPlayer() && userid != g_EngineClient->GetLocalPlayer())
			Visuals::DrawCapsuleOverlay(userid, 0.8f);
	}

	if (g_Options.misc_logevents)
	{
		auto HitgroupToString = [](int hitgroup) -> std::string
		{
			switch (hitgroup)
			{
			case HITGROUP_GENERIC:
				return "generic";
			case HITGROUP_HEAD:
				return "head";
			case HITGROUP_CHEST:
				return "chest";
			case HITGROUP_STOMACH:
				return "stomach";
			case HITGROUP_LEFTARM:
				return "left arm";
			case HITGROUP_RIGHTARM:
				return "right arm";
			case HITGROUP_LEFTLEG:
				return "left leg";
			case HITGROUP_RIGHTLEG:
				return "right leg";
			case HITGROUP_GEAR:
				return "gear";
			}
			return "generic";
		};

		std::stringstream msg;

		auto enemy = event->GetInt("userid");
		auto attacker = event->GetInt("attacker");
		auto remaining_health = event->GetString("health");
		auto dmg_to_health = event->GetString("dmg_health");
		auto hitgroup = event->GetInt("hitgroup");

		auto enemy_index = g_EngineClient->GetPlayerForUserID(enemy);
		auto attacker_index = g_EngineClient->GetPlayerForUserID(attacker);
		auto pEnemy = C_BasePlayer::GetPlayerByIndex(enemy_index);
		auto pAttacker = C_BasePlayer::GetPlayerByIndex(attacker_index);

		player_info_t attacker_info;
		player_info_t enemy_info;

		if (pEnemy && pAttacker && g_EngineClient->GetPlayerInfo(attacker_index, &attacker_info) && g_EngineClient->GetPlayerInfo(enemy_index, &enemy_info))
		{
			if (attacker_index != g_EngineClient->GetLocalPlayer())
				return;

			std::string szHitgroup = HitgroupToString(hitgroup);

			msg << "Hit " << enemy_info.szName << " in the " << szHitgroup << " for " << dmg_to_health << " dmg " << "(" << remaining_health << " health remaining)";

			Utils::EventLog(msg.str());
		}
	}
	else
		if (eventLog.size() > 0)
			eventLog.clear();

	if (g_Options.hvh_resolver)
	{
		int32_t userid = event->GetInt("userid");
		auto player = C_BasePlayer::GetPlayerByUserId(userid);
		if (!player)
			return;

		int32_t idx = player->EntIndex();
		auto &player_recs = Resolver::Get().arr_infos[idx];

		if (!player->IsDormant())
		{
			int32_t local_id = g_EngineClient->GetLocalPlayer();
			int32_t attacker = g_EngineClient->GetPlayerForUserID(event->GetInt("attacker"));

			if (attacker == local_id)
			{
				bool airJump = !(player->m_fFlags() & FL_ONGROUND) && player->m_vecVelocity().Length2D() > 1.0f;
				int32_t tickcount = g_GlobalVars->tickcount;

				if (tickHitWall == tickcount)
				{
					player_recs.m_nShotsMissed = originalShotsMissed;
					player_recs.m_nCorrectedFakewalkIdx = originalCorrectedFakewalkIdx;
				}
				if (tickcount != tickHitPlayer)
				{
					tickHitPlayer = tickcount;
					player_recs.m_nShotsMissed = 0;

					if (!airJump)
					{
						if (++player_recs.m_nCorrectedFakewalkIdx > 7)
							player_recs.m_nCorrectedFakewalkIdx = 0;
					}
				}
			}
		}
	}
}

int PlayerHurtEvent::GetEventDebugID(void)
{
	return EVENT_DEBUG_ID_INIT;
}

void PlayerHurtEvent::RegisterSelf()
{
	g_GameEvents->AddListener(this, "player_hurt", false);
}

void PlayerHurtEvent::UnregisterSelf()
{
	g_GameEvents->RemoveListener(this);
}

void PlayerHurtEvent::Paint(void)
{
	static int width = 0, height = 0;
	if (width == 0 || height == 0)
		g_EngineClient->GetScreenSize(width, height);

	if (eventLog.size() > 6)
		eventLog.erase(eventLog.begin() + 1);

	float alpha = 0.f;

	if (g_Options.visuals_others_hitmarker)
	{
		for (size_t i = 0; i < hitMarkerInfo.size(); i++)
		{
			float diff = hitMarkerInfo.at(i).m_flExpTime - g_GlobalVars->curtime;

			if (diff < 0.f)
			{
				hitMarkerInfo.erase(hitMarkerInfo.begin() + i);
				continue;
			}

			int dist = 24;

			float ratio = 1.f - (diff / 0.8f);
			alpha = 0.8f - diff / 0.8f;

			Visuals::DrawString(Visuals::ui_font, width / 2 + 6 + ratio * dist / 2, height / 2 + 6 + ratio * dist, Color(255, 255, 0, (int)(alpha * 255.f)), FONT_LEFT, std::to_string(hitMarkerInfo.at(i).m_iDmg).c_str());
		}

		if (hitMarkerInfo.size() > 0)
		{
			int lineSize = 12;
			g_VGuiSurface->DrawSetColor(Color(255, 255, 255, (int)(alpha * 255.f)));
			g_VGuiSurface->DrawLine(width / 2 - lineSize / 2, height / 2 - lineSize / 2, width / 2 + lineSize / 2, height / 2 + lineSize / 2);
			g_VGuiSurface->DrawLine(width / 2 + lineSize / 2, height / 2 - lineSize / 2, width / 2 - lineSize / 2, height / 2 + lineSize / 2);
		}
	}

	if (g_Options.misc_logevents)
	{
		int x = 8, y = 7;

		for (size_t i = 0; i < eventLog.size(); ++i) // valve code at it's finest (lmao) https://github.com/VSES/SourceEngine2007/blob/master/se2007/engine/console.cpp#L978-L1010
		{
			eventLog[i].m_flTime -= g_GlobalVars->frametime;

			int fontTall = g_VGuiSurface->GetFontTall(Visuals::log_font) + 1;

			if (eventLog[i].m_flTime < .5f)
			{
				float f = std::clamp(eventLog[i].m_flTime, 0.0f, .5f) / .5f;

				if (i == 0)
				{
					eventLog[i].m_Color.SetAlpha((int)(f * 255.0f));

					if (f < 0.2f)
					{
						y -= fontTall * (1.0f - f / 0.2f) - 2;
					}
				}
			}
			else
			{
				eventLog[i].m_Color.SetAlpha(255);
			}

			if (i == 0 && eventLog[i].m_Color.a() <= .1f)
			{
				eventLog.erase(eventLog.begin() + i);
				eventLog[i].m_flTime = 0.75f;
			}

			Visuals::DrawString(Visuals::log_font, x, y + (i * 16), eventLog[i].m_Color, FONT_LEFT, eventLog[i].m_Text.c_str());
		}
	}
}