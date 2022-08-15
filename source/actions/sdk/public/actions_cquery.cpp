#include "actions_processor.h"
#include "actions_procs.h"
#include "actions_cquery.h"

#include "NextBotContextualQueryInterface.h"

SH_DECL_MANUALHOOK2(ShouldPickUp, 0, 0, 4, QueryResultType, INextBot*, CBaseEntity*);
SH_DECL_MANUALHOOK1(ShouldHurry, 0, 0, 4, QueryResultType, INextBot*);
SH_DECL_MANUALHOOK1(ShouldRetreat, 0, 0, 4, QueryResultType, INextBot*);
SH_DECL_MANUALHOOK2(ShouldAttack, 0, 0, 4, QueryResultType, INextBot*, const CKnownEntity*);
SH_DECL_MANUALHOOK2(IsHindrance, 0, 0, 4, QueryResultType, INextBot*, CBaseEntity*);
SH_DECL_MANUALHOOK2(SelectTargetPoint, 0, 0, 4, Vector, INextBot*, CBaseCombatCharacter*);
SH_DECL_MANUALHOOK2(IsPositionAllowed, 0, 0, 4, QueryResultType, INextBot*, Vector&);
SH_DECL_MANUALHOOK4(SelectMoreDangerousThreat, 0, 0, 4, CKnownEntity*, INextBot*, CBaseCombatCharacter*, CKnownEntity*, CKnownEntity*);

ActionContextualProcessor::ActionContextualProcessor(CBaseEntity* entity, Action<void>* action) : ActionProcessor(entity, action)
{
}

ActionContextualProcessor::ActionContextualProcessor(Action<void>* action) : ActionProcessor(static_cast<CBaseEntity*>(action->GetActor()), action)
{
}

void ActionContextualProcessor::StartProcessors()
{
    START_PROCESSOR(ShouldPickUp, shouldPickUp);
	START_PROCESSOR(ShouldHurry, shouldHurry);
	START_PROCESSOR(IsHindrance, isHindrance);
	START_PROCESSOR(SelectTargetPoint, selectTargetPoint);
	START_PROCESSOR(IsPositionAllowed, isPositionAllowed);
	START_PROCESSOR(SelectMoreDangerousThreat, selectMoreDangerousThreat);
	START_PROCESSOR(ShouldRetreat, shouldRetreat);
	START_PROCESSOR(ShouldAttack, shouldAttack);

	ActionProcessor::StartProcessors();
}

const bool ActionContextualProcessor::ConfigureHooks()
{
	RECONFIGURE_MANUALHOOK(ShouldPickUp);
	RECONFIGURE_MANUALHOOK(ShouldHurry);
	RECONFIGURE_MANUALHOOK(IsHindrance);
	RECONFIGURE_MANUALHOOK(SelectTargetPoint);
	RECONFIGURE_MANUALHOOK(IsPositionAllowed);
	RECONFIGURE_MANUALHOOK(SelectMoreDangerousThreat);
	RECONFIGURE_MANUALHOOK(ShouldRetreat);
	RECONFIGURE_MANUALHOOK(ShouldAttack);

	return ActionProcessor::ConfigureHooks();
}