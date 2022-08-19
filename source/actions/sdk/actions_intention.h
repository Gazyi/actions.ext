#pragma once

#include "NextBotEventResponderInterface.h"
#include "NextBotInterface.h"

extern void ExecuteProcessor(CBaseEntity* entity, Action<void>* action);
SH_DECL_MANUALHOOK0_void(OnIntentionReset, 0, 0, 0);

class NextBotIntention : public IIntention
{
public:
	Behavior<void>* behavior;

	Action<void>* ActionContainedResponder() 
	{ 
		return static_cast<decltype(behavior)>(FirstContainedResponder())->m_action; 
	}
};

void OnIntentionReset()
{
	NextBotIntention* pIntention = META_IFACEPTR(NextBotIntention);

	if (pIntention->ActionContainedResponder() == nullptr)
		return;

	ExecuteProcessor((CBaseEntity*)(pIntention->GetBot()->GetEntity()), pIntention->ActionContainedResponder());
}

void HookIntention(const char* name, fastdelegate::FastDelegate<void> fastDelegate = SH_STATIC(OnIntentionReset))
{
	void* addr = static_cast<void*>(GetOffsetsManager()->RequestAddress(name));

	if (addr == nullptr)
	{
		LOGERROR("Failed to find address for \"%s\" key. Check your gamedata...", name);
		return;
	}
	
	SH_ADD_MANUALDVPHOOK(OnIntentionReset, addr, fastDelegate, true);
}

void CreateHooks()
{
	int32_t intention_reset = GetOffsetsManager()->RequestOffset("IIntention::Reset");

	if (GetOffsetsManager()->HaveFailedRequest())
		return;

	SH_MANUALHOOK_RECONFIGURE(OnIntentionReset, intention_reset, 0, 0);

	HookIntention("CBotNPCArcherIntention::Reset");
	HookIntention("CBotNPCDecoyIntention::Reset");
	HookIntention("CEyeballBossIntention::Reset");
	HookIntention("CGhostIntention::Reset");
	HookIntention("CHeadlessHatmanIntention::Reset");
	HookIntention("CMerasmusIntention::Reset");
	HookIntention("CRobotIntention::Reset");
	HookIntention("CSimpleBotIntention::Reset");
	HookIntention("CTFBotIntention::Reset");
	HookIntention("CZombieIntention::Reset");
}