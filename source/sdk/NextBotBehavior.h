#pragma once

// NextBotBehaviorEngine.h
// Behavioral system constructed from Actions
// Author: Michael Booth, April 2006
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved

#ifndef _BEHAVIOR_ENGINE_H_
#define _BEHAVIOR_ENGINE_H_

#include "NextBotEventResponderInterface.h"
#include "NextBotContextualQueryInterface.h"

#include "fmtstr.h"

//#define DEBUG_BEHAVIOR_MEMORY

/**
 * Notes:
 *
 * By using return results to cause transitions, we ensure the atomic-ness
 * of these transitions. For instance, it is not possible to change to a
 * new Action and continue execution of code in the current Action.
 *
 * Creation and deletion of Actions during transitions allows passing of
 * type-safe arguments between Actions via constructors.
 *
 * Events are propagated to each Action in the hierarchy.  If an
 * action is suspended for another action, it STILL RECEIVES EVENTS
 * that are not handled by the events "above it" in the suspend stack.
 * In other words, the active Action gets the first response, and if it
 * returns CONTINUE, the Action buried beneath it can process it,
 * and so on deeper into the stack of suspended Actions.
 *
 * About events:
 * It is not possible to have event handlers instantaneously change
 * state upon return due to out-of-order and recurrence issues, not
 * to mention deleting the state out from under itself.  Therefore,
 * events return DESIRED results, and the highest priority result
 * is executed at the next Update().
 *
 * About buried Actions causing SUSPEND_FOR results:
 * If a buried Action reacts to an event by returning a SUSPEND_FOR,
 * the new interrupting Action is put at the TOP of the stack, burying
 * whatever Action was there.
 *
 */


 // forward declaration
template < typename Actor > class Action;

/**
 * The possible consequences of an Action
 */
enum ActionResultType
{
	CONTINUE,			// continue executing this action next frame - nothing has changed
	CHANGE_TO,			// change actions next frame
	SUSPEND_FOR,		// put the current action on hold for the new action
	DONE,				// this action has finished, resume suspended action
	SUSTAIN,			// for use with event handlers - a way to say "It's important to keep doing what I'm doing"
};


//----------------------------------------------------------------------------------------------
/**
 * Actions and Event processors return results derived from this class.
 * Do not assemble this yourself - use the Continue(), ChangeTo(), Done(), and SuspendFor()
 * methods within Action.
 */
template < typename Actor >
struct IActionResult
{
	IActionResult(ActionResultType type = CONTINUE, Action< Actor >* action = NULL, const char* reason = NULL)
	{
		m_type = type;
		m_action = action;
		m_reason = reason;
	}

	bool IsDone(void) const
	{
		return (m_type == DONE);
	}

	bool IsContinue(void) const
	{
		return (m_type == CONTINUE);
	}

	bool IsRequestingChange(void) const
	{
		return (m_type == CHANGE_TO || m_type == SUSPEND_FOR || m_type == DONE);
	}

	const char* GetTypeName(void) const
	{
		switch (m_type)
		{
		case CHANGE_TO:		return "CHANGE_TO";
		case SUSPEND_FOR:	return "SUSPEND_FOR";
		case DONE:			return "DONE";
		case SUSTAIN:		return "SUSTAIN";

		default:
		case CONTINUE:		return "CONTINUE";
		}
	}

	ActionResultType m_type;
	Action< Actor >* m_action;
	const char* m_reason;
};


//----------------------------------------------------------------------------------------------
/**
 * When an Action is executed it returns this result.
 * Do not assemble this yourself - use the Continue(), ChangeTo(), Done(), and SuspendFor()
 * methods within Action.
 */
template < typename Actor >
struct ActionResult : public IActionResult< Actor >
{
	// this is derived from IActionResult to ensure that ActionResult and EventDesiredResult cannot be silently converted
	ActionResult(ActionResultType type = CONTINUE, Action< Actor >* action = NULL, const char* reason = NULL) : IActionResult< Actor >(type, action, reason) { }
};


//----------------------------------------------------------------------------------------------
/**
 * When an event is processed, it returns this DESIRED result,
 * which may or MAY NOT happen, depending on other event results
 * that occur simultaneously.
 * Do not assemble this yourself - use the TryContinue(), TryChangeTo(), TryDone(), TrySustain(),
 * and TrySuspendFor() methods within Action.
 */
enum EventResultPriorityType
{
	RESULT_NONE,		// no result
	RESULT_TRY,			// use this result, or toss it out, either is ok
	RESULT_IMPORTANT,	// try extra-hard to use this result
	RESULT_CRITICAL		// this result must be used - emit an error if it can't be
};

template < typename Actor >
struct EventDesiredResult : public IActionResult< Actor >
{
	EventDesiredResult(ActionResultType type = CONTINUE, Action< Actor >* action = NULL, EventResultPriorityType priority = RESULT_TRY, const char* reason = NULL) : IActionResult< Actor >(type, action, reason)
	{
		m_priority = priority;
	}

	EventResultPriorityType m_priority;
};

//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
/**
 * A Behavior is the root of an Action hierarchy as well as its container/manager.
 * Instantiate a Behavior with the root Action of your behavioral system, and
 * call Behavior::Update() to drive it.
 */
template < typename Actor >
class Behavior : public INextBotEventResponder, public IContextualQuery
{
public:

	Behavior(Action< Actor >* initialAction, const char* name = "") : m_name(name)
	{
		m_action = initialAction;
	}

	virtual ~Behavior()
	{
		// dig down to the bottom of the action stack and delete 
		// that, so we don't leak action memory since action 
		// destructors intentionally don't delete actions
		// "buried" underneath them.
		Action< Actor >* bottomAction;
		for (bottomAction = m_action; bottomAction && bottomAction->m_buriedUnderMe; bottomAction = bottomAction->m_buriedUnderMe)
			;

		if (bottomAction)
		{
			delete bottomAction;
		}
	}

	/**
	 * Reset this Behavior with the given Action. If this Behavior
	 * was already running, this will delete all current Actions and
	 * restart the Behavior with the new one.
	 */
	void Reset(Action< Actor >* action)
	{
		// find "bottom" action (see comment in destructor)
		Action< Actor >* bottomAction;
		for (bottomAction = m_action; bottomAction && bottomAction->m_buriedUnderMe; bottomAction = bottomAction->m_buriedUnderMe)
			;

		if (bottomAction)
		{
			delete bottomAction;
		}

		m_action = action;
	}

	/**
	 * Return true if this Behavior contains no actions
	 */
	bool IsEmpty(void) const
	{
		return m_action == NULL;
	}

	/**
	 * Execute this Behavior
	 */
	void Update(Actor* me, float interval)
	{
		if (me == NULL || IsEmpty())
		{
			return;
		}

		m_action = m_action->ApplyResult(me, this, m_action->InvokeUpdate(me, this, interval));
	}

	/**
	 * If this Behavior has not been Update'd in a long time,
	 * call Resume() to let the system know its internal state may
	 * be out of date.
	 */
	void Resume(Actor* me)
	{
		if (me == NULL || IsEmpty())
		{
			return;
		}

		m_action = m_action->ApplyResult(me, this, m_action->OnResume(me, NULL));
	}

	const char* GetName(void) const
	{
		return m_name;
	}

	// INextBotEventResponder propagation ----------------------------------------------------------------------
	virtual INextBotEventResponder* FirstContainedResponder(void) const
	{
		return m_action;
	}

	virtual INextBotEventResponder* NextContainedResponder(INextBotEventResponder* current) const
	{
		return NULL;
	}

	// IContextualQuery propagation ----------------------------------------------------------------------------
	virtual QueryResultType ShouldPickUp(const INextBot* me, CBaseEntity* item) const		// if the desired item was available right now, should we pick it up?
	{
		QueryResultType result = ANSWER_UNDEFINED;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == ANSWER_UNDEFINED)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == ANSWER_UNDEFINED)
				{
					result = action->ShouldPickUp(me, item);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}

	virtual QueryResultType ShouldHurry(const INextBot* me) const					// are we in a hurry?
	{
		QueryResultType result = ANSWER_UNDEFINED;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == ANSWER_UNDEFINED)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == ANSWER_UNDEFINED)
				{
					result = action->ShouldHurry(me);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}

	virtual QueryResultType IsHindrance(const INextBot* me, CBaseEntity* blocker) const	// return true if we should wait for 'blocker' that is across our path somewhere up ahead.
	{
		QueryResultType result = ANSWER_UNDEFINED;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == ANSWER_UNDEFINED)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == ANSWER_UNDEFINED)
				{
					result = action->IsHindrance(me, blocker);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}


	virtual Vector SelectTargetPoint(const INextBot* me, const CBaseCombatCharacter* subject) const		// given a subject, return the world space position we should aim at
	{
		Vector result = vec3_origin;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == vec3_origin)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == vec3_origin)
				{
					result = action->SelectTargetPoint(me, subject);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}


	/**
	 * Allow bot to approve of positions game movement tries to put him into.
	 * This is most useful for bots derived from CBasePlayer that go through
	 * the player movement system.
	 */
	virtual QueryResultType IsPositionAllowed(const INextBot* me, const Vector& pos) const
	{
		QueryResultType result = ANSWER_UNDEFINED;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == ANSWER_UNDEFINED)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == ANSWER_UNDEFINED)
				{
					result = action->IsPositionAllowed(me, pos);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}

	virtual const CKnownEntity* SelectMoreDangerousThreat(const INextBot* me, const CBaseCombatCharacter* subject, const CKnownEntity* threat1, const CKnownEntity* threat2) const	// return the more dangerous of the two threats, or NULL if we have no opinion
	{
		const CKnownEntity* result = NULL;

		if (m_action)
		{
			// find innermost child action
			Action< Actor >* action;
			for (action = m_action; action->m_child; action = action->m_child)
				;

			// work our way through our containers
			while (action && result == NULL)
			{
				Action< Actor >* containingAction = action->m_parent;

				// work our way up the stack
				while (action && result == NULL)
				{
					result = action->SelectMoreDangerousThreat(me, subject, threat1, threat2);
					action = action->GetActionBuriedUnderMe();
				}

				action = containingAction;
			}
		}

		return result;
	}


private:
	Action< Actor >* m_action;

#define MAX_NAME_LENGTH 32
	CFmtStrN< MAX_NAME_LENGTH > m_name;
};


//----------------------------------------------------------------------------------------------
/**
 * Something an Actor does.
 * Actions can contain Actions, representing the precise context of the Actor's behavior.
 * A system of Actions is contained within a Behavior, which acts as the manager
 * of the Action system.
 */
template < typename Actor >
class Action : public INextBotEventResponder, public IContextualQuery
{
public:

	Action(void);
	virtual ~Action();

	virtual const char* GetName(void) const = 0;		// return name of this action
	virtual bool IsNamed(const char* name) const;		// return true if given name matches the name of this Action
	virtual const char* GetFullName(void) const;		// return a temporary string showing the full lineage of this one action
	Actor* GetActor(void) const;						// return the Actor performing this Action (valid just before OnStart() is invoked)

	//-----------------------------------------------------------------------------------------
	/**
	 * Try to start the Action. Result is immediately processed,
	 * which can cause an immediate transition, another OnStart(), etc.
	 * An Action can count on each OnStart() being followed (eventually) with an OnEnd().
	 */
	virtual ActionResult< Actor >	OnStart(Actor* me, Action< Actor >* priorAction) { return Continue(); }

	/**
	 * Do the work of the Action. It is possible for Update to not be
	 * called between a given OnStart/OnEnd pair due to immediate transitions.
	 */
	virtual ActionResult< Actor >	Update(Actor* me, float interval) { return Continue(); }

	// Invoked when an Action is ended for any reason
	virtual void					OnEnd(Actor* me, Action< Actor >* nextAction) {  }

	/*
	 * When an Action is suspended by a new action.
	 * Note that only CONTINUE and DONE are valid results.  All other results will
	 * be considered as a CONTINUE.
	 */
	virtual ActionResult< Actor >	OnSuspend(Actor* me, Action< Actor >* interruptingAction) { return Continue(); }

	// When an Action is resumed after being suspended
	virtual ActionResult< Actor >	OnResume(Actor* me, Action< Actor >* interruptingAction) { return Continue(); }

	/**
	 * To cause a state change, use these methods to create an ActionResult to
	 * return from OnStart, Update, or OnResume.
	 */
	ActionResult< Actor > Continue(void) const;
	ActionResult< Actor > ChangeTo(Action< Actor >* action, const char* reason = NULL) const;
	ActionResult< Actor > SuspendFor(Action< Actor >* action, const char* reason = NULL) const;
	ActionResult< Actor > Done(const char* reason = NULL) const;

	// create and return an Action to start as sub-action within this Action when it starts
	virtual Action< Actor >* InitialContainedAction(Actor* me) { return NULL; }

	//-----------------------------------------------------------------------------------------
	/**
	 * Override the event handler methods below to respond to events that occur during this Action
	 * NOTE: These are identical to the events in INextBotEventResponder with the addition
	 * of an actor argument and a return result. Their translators are located in the private area
	 * below.
	 */
	virtual EventDesiredResult< Actor > OnLeaveGround(Actor* me, CBaseEntity* ground) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnLandOnGround(Actor* me, CBaseEntity* ground) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnContact(Actor* me, CBaseEntity* other, CGameTrace* result = NULL) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnMoveToSuccess(Actor* me, const Path* path) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnMoveToFailure(Actor* me, const Path* path, MoveToFailureType reason) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnStuck(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnUnStuck(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnPostureChanged(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnAnimationActivityComplete(Actor* me, int activity) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnAnimationActivityInterrupted(Actor* me, int activity) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnAnimationEvent(Actor* me, animevent_t* event) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnIgnite(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnInjured(Actor* me, const CTakeDamageInfo& info) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnKilled(Actor* me, const CTakeDamageInfo& info) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnOtherKilled(Actor* me, CBaseCombatCharacter* victim, const CTakeDamageInfo& info) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnSight(Actor* me, CBaseEntity* subject) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnLostSight(Actor* me, CBaseEntity* subject) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnThreatChanged(Actor* me, CBaseEntity* subject) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnSound(Actor* me, CBaseEntity* source, const Vector& pos, KeyValues* keys) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnSpokeConcept(Actor* me, CBaseCombatCharacter* who, AIConcept_t concept, AI_Response* response, void* last) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnNavAreaChanged(Actor* me, CNavArea* newArea, CNavArea* oldArea) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnModelChanged(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnPickUp(Actor* me, CBaseEntity* item, CBaseCombatCharacter* giver) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnDrop(Actor* me, CBaseEntity* item) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnShoved(Actor* me, CBaseEntity* pusher) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnBlinded(Actor* me, CBaseEntity* blinder) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnHitByVomitJar(Actor* me, CBaseEntity* owner) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnEnteredSpit(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandAttack(Actor* me, CBaseEntity* victim) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandAssault(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandApproach(Actor* me, const Vector& pos, float range) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandApproach(Actor* me, CBaseEntity* goal) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandRetreat(Actor* me, CBaseEntity* threat, float range) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandPause(Actor* me, float duration) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandResume(Actor* me) { return TryContinue(); }
	virtual EventDesiredResult< Actor > OnCommandString(Actor* me, const char* command) { return TryContinue(); }

	/**
	 * Event handlers must return one of these.
	 */
	EventDesiredResult< Actor > TryContinue(EventResultPriorityType priority = RESULT_TRY) const;
	EventDesiredResult< Actor > TryChangeTo(Action< Actor >* action, EventResultPriorityType priority = RESULT_TRY, const char* reason = NULL) const;
	EventDesiredResult< Actor > TrySuspendFor(Action< Actor >* action, EventResultPriorityType priority = RESULT_TRY, const char* reason = NULL) const;
	EventDesiredResult< Actor > TryDone(EventResultPriorityType priority = RESULT_TRY, const char* reason = NULL) const;
	EventDesiredResult< Actor > TryToSustain(EventResultPriorityType priority = RESULT_TRY, const char* reason = NULL) const;


	//-----------------------------------------------------------------------------------------
	Action< Actor >* GetActiveChildAction(void) const;
	Action< Actor >* GetParentAction(void) const;			// the Action that I'm running inside of

	bool IsSuspended(void) const;						// return true if we are currently suspended for another Action

	const char* DebugString(void) const;								// return a temporary string describing the current action stack for debugging

	/**
	 * Sometimes we want to pass through other NextBots. OnContact() will always
	 * be invoked, but collision resolution can be skipped if this
	 * method returns false.
	 */
	virtual bool IsAbleToBlockMovementOf(const INextBot* botInMotion) const { return true; }

	// INextBotEventResponder propagation ----------------------------------------------------------------------
	virtual INextBotEventResponder* FirstContainedResponder(void) const;
	virtual INextBotEventResponder* NextContainedResponder(INextBotEventResponder* current) const;


private:

	/**
	 * These macros are used below to translate INextBotEventResponder event methods
	 * into Action event handler methods
	 */
#define PROCESS_EVENT( METHOD )							\
		{													\
			if ( !m_isStarted )								\
				return;										\
															\
			Action< Actor > *_action = this;				\
			EventDesiredResult< Actor > _result;			\
															\
			while( _action )								\
			{												\
				_result = _action->METHOD( m_actor );	\
				if ( !_result.IsContinue() )				\
					break;									\
				_action = _action->GetActionBuriedUnderMe();		\
			}												\
															\
			if ( _action )									\
			{												\
															\
				_action->StorePendingEventResult( _result, #METHOD );	\
			}												\
															\
			INextBotEventResponder::METHOD();			\
		}


#define PROCESS_EVENT_WITH_1_ARG( METHOD, ARG1 )		\
		{													\
			if ( !m_isStarted )								\
				return;										\
															\
			Action< Actor > *_action = this;				\
			EventDesiredResult< Actor > _result;			\
															\
			while( _action )								\
			{												\
				_result = _action->METHOD( m_actor, ARG1 );		\
				if ( !_result.IsContinue() )				\
					break;									\
				_action = _action->GetActionBuriedUnderMe();		\
			}												\
															\
			if ( _action )									\
			{												\
															\
				_action->StorePendingEventResult( _result, #METHOD );	\
			}												\
															\
			INextBotEventResponder::METHOD( ARG1 );		\
		}


#define PROCESS_EVENT_WITH_2_ARGS( METHOD, ARG1, ARG2 )	\
		{													\
			if ( !m_isStarted )								\
				return;										\
															\
			Action< Actor > *_action = this;				\
			EventDesiredResult< Actor > _result;			\
															\
			while( _action )								\
			{												\
				_result = _action->METHOD( m_actor, ARG1, ARG2 );		\
				if ( !_result.IsContinue() )				\
					break;									\
				_action = _action->GetActionBuriedUnderMe();				\
			}												\
															\
			if ( _action )									\
			{												\
															\
				_action->StorePendingEventResult( _result, #METHOD );	\
			}												\
															\
			INextBotEventResponder::METHOD( ARG1, ARG2 );			\
		}


#define PROCESS_EVENT_WITH_3_ARGS( METHOD, ARG1, ARG2, ARG3 )	\
		{													\
			if ( !m_isStarted )								\
				return;										\
															\
			Action< Actor > *_action = this;				\
			EventDesiredResult< Actor > _result;			\
															\
			while( _action )								\
			{												\
				_result = _action->METHOD( m_actor, ARG1, ARG2, ARG3 );		\
				if ( !_result.IsContinue() )				\
					break;									\
				_action = _action->GetActionBuriedUnderMe();				\
			}												\
															\
			if ( _action )									\
			{												\
															\
				_action->StorePendingEventResult( _result, #METHOD );	\
			}												\
															\
			INextBotEventResponder::METHOD( ARG1, ARG2, ARG3 );			\
		}


#define PROCESS_EVENT_WITH_4_ARGS( METHOD, ARG1, ARG2, ARG3, ARG4 )	\
		{													\
			if ( !m_isStarted )								\
				return;										\
															\
			Action< Actor > *_action = this;				\
			EventDesiredResult< Actor > _result;			\
															\
			while( _action )								\
			{												\
				_result = _action->METHOD( m_actor, ARG1, ARG2, ARG3, ARG4 );		\
				if ( !_result.IsContinue() )				\
					break;									\
				_action = _action->GetActionBuriedUnderMe();				\
			}												\
															\
			if ( _action )									\
			{												\
															\
				_action->StorePendingEventResult( _result, #METHOD );	\
			}												\
															\
			INextBotEventResponder::METHOD( ARG1, ARG2, ARG3, ARG4 );			\
		}


	 /**
	  * Translate incoming events into Action events
	  * DO NOT OVERRIDE THESE METHODS
	  */
	virtual void OnLeaveGround(CBaseEntity* ground) override { PROCESS_EVENT_WITH_1_ARG(OnLeaveGround, ground); }
	virtual void OnLandOnGround(CBaseEntity* ground) override { PROCESS_EVENT_WITH_1_ARG(OnLandOnGround, ground); }
	virtual void OnContact(CBaseEntity* other, CGameTrace* result) override { PROCESS_EVENT_WITH_2_ARGS(OnContact, other, result); }
	virtual void OnMoveToSuccess(const Path* path) override { PROCESS_EVENT_WITH_1_ARG(OnMoveToSuccess, path); }
	virtual void OnMoveToFailure(const Path* path, MoveToFailureType reason) override { PROCESS_EVENT_WITH_2_ARGS(OnMoveToFailure, path, reason); }
	virtual void OnStuck(void) override { PROCESS_EVENT(OnStuck); }
	virtual void OnUnStuck(void) override { PROCESS_EVENT(OnUnStuck); }
	virtual void OnPostureChanged(void) override { PROCESS_EVENT(OnPostureChanged); }
	virtual void OnAnimationActivityComplete(int activity) override { PROCESS_EVENT_WITH_1_ARG(OnAnimationActivityComplete, activity); }
	virtual void OnAnimationActivityInterrupted(int activity) override { PROCESS_EVENT_WITH_1_ARG(OnAnimationActivityInterrupted, activity); }
	virtual void OnAnimationEvent(animevent_t* event) override { PROCESS_EVENT_WITH_1_ARG(OnAnimationEvent, event); }
	virtual void OnIgnite(void) override { PROCESS_EVENT(OnIgnite); }
	virtual void OnInjured(const CTakeDamageInfo& info) override { PROCESS_EVENT_WITH_1_ARG(OnInjured, info); }
	virtual void OnKilled(const CTakeDamageInfo& info) override { PROCESS_EVENT_WITH_1_ARG(OnKilled, info); }
	virtual void OnOtherKilled(CBaseCombatCharacter* victim, const CTakeDamageInfo& info) override { PROCESS_EVENT_WITH_2_ARGS(OnOtherKilled, victim, info); }
	virtual void OnSight(CBaseEntity* subject) override { PROCESS_EVENT_WITH_1_ARG(OnSight, subject); }
	virtual void OnLostSight(CBaseEntity* subject) override { PROCESS_EVENT_WITH_1_ARG(OnLostSight, subject); }
	virtual void OnThreatChanged(CBaseEntity* subject) override { PROCESS_EVENT_WITH_1_ARG(OnThreatChanged, subject); }
	virtual void OnSound(CBaseEntity* source, const Vector& pos, KeyValues* keys) override { PROCESS_EVENT_WITH_3_ARGS(OnSound, source, pos, keys); }
	virtual void OnSpokeConcept(CBaseCombatCharacter* who, AIConcept_t pconcept, AI_Response* response, void* unknown) override { PROCESS_EVENT_WITH_4_ARGS(OnSpokeConcept, who, pconcept, response, unknown); }
	virtual void OnNavAreaChanged(CNavArea* newArea, CNavArea* oldArea) override { PROCESS_EVENT_WITH_2_ARGS(OnNavAreaChanged, newArea, oldArea); }
	virtual void OnModelChanged(void) override { PROCESS_EVENT(OnModelChanged); }
	virtual void OnPickUp(CBaseEntity* item, CBaseCombatCharacter* giver) override { PROCESS_EVENT_WITH_2_ARGS(OnPickUp, item, giver); }
	virtual void OnDrop(CBaseEntity* item) override { PROCESS_EVENT_WITH_1_ARG(OnDrop, item); }
	virtual void OnShoved(CBaseEntity* pusher) override { PROCESS_EVENT_WITH_1_ARG(OnShoved, pusher); }
	virtual void OnBlinded(CBaseEntity* blinder) override { PROCESS_EVENT_WITH_1_ARG(OnBlinded, blinder); }
	virtual void OnCommandAttack(CBaseEntity* victim) override { PROCESS_EVENT_WITH_1_ARG(OnCommandAttack, victim); }
	virtual void OnCommandApproach(const Vector& pos, float range) override { PROCESS_EVENT_WITH_2_ARGS(OnCommandApproach, pos, range); }
	virtual void OnCommandApproach(CBaseEntity* goal) override { PROCESS_EVENT_WITH_1_ARG(OnCommandApproach, goal); }
	virtual void OnCommandRetreat(CBaseEntity* threat, float range) override { PROCESS_EVENT_WITH_2_ARGS(OnCommandRetreat, threat, range); }
	virtual void OnCommandPause(float duration) override { PROCESS_EVENT_WITH_1_ARG(OnCommandPause, duration); }
	virtual void OnCommandResume(void) override { PROCESS_EVENT(OnCommandResume); }
	virtual void OnCommandAssault() override { PROCESS_EVENT(OnCommandAssault); }
	virtual void OnHitByVomitJar(CBaseEntity* owner) override { PROCESS_EVENT_WITH_1_ARG(OnHitByVomitJar, owner); }
	virtual void OnEnteredSpit() override { PROCESS_EVENT(OnEnteredSpit); }
	virtual void OnCommandString(const char* command) override { PROCESS_EVENT_WITH_1_ARG(OnCommandString, command); }

public:
	friend class Behavior< Actor>;							// the containing Behavior class
	Behavior< Actor >* m_behavior;							// the Behavior this Action is part of

	Action< Actor >* m_parent;								// the Action that contains us
	Action< Actor >* m_child;								// the ACTIVE Action we contain, top of the stack. Use m_buriedUnderMe, m_coveringMe on the child to traverse to other suspended children

	Action< Actor >* m_buriedUnderMe;						// the Action just "under" us in the stack that we will resume to when we finish
	Action< Actor >* m_coveringMe;							// the Action just "above" us in the stack that will resume to us when it finishes

	Actor* m_actor;											// only valid after OnStart()
	mutable EventDesiredResult< Actor > m_eventResult;		// set by event handlers
	bool m_isStarted;										// Action doesn't start until OnStart() is invoked
	bool m_isSuspended;										// are we suspended for another Action

	Action< Actor >* GetActionBuriedUnderMe(void) const	// return Action just "under" us that we will resume to when we finish
	{
		return m_buriedUnderMe;
	}

	Action< Actor >* GetActionCoveringMe(void) const		// return Action just "above" us that will resume to us when it finishes
	{
		return m_coveringMe;
	}

public:
	/**
	 * If any Action buried underneath me has either exited
	 * or is changing to a different Action, we're "out of scope"
	 */
	bool IsOutOfScope(void) const
	{
		for (Action< Actor >* under = GetActionBuriedUnderMe(); under; under = under->GetActionBuriedUnderMe())
		{
			if (under->m_eventResult.m_type == CHANGE_TO ||
				under->m_eventResult.m_type == DONE)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Process any pending events with the stack. This is called
	 * by the active Action on the top of the stack, and walks
	 * through any buried Actions checking for pending event results.
	 */
	ActionResult< Actor > ProcessPendingEvents(void) const
	{
		// if an event has requested a change, honor it
		if (m_eventResult.IsRequestingChange())
		{
			ActionResult< Actor > result(m_eventResult.m_type, m_eventResult.m_action, m_eventResult.m_reason);

			// clear event result in case this change is a suspend and we later resume this action
			m_eventResult = TryContinue(RESULT_NONE);

			return result;
		}

		// check for pending event changes buried in the stack
		Action< Actor >* under = GetActionBuriedUnderMe();
		while (under)
		{
			if (under->m_eventResult.m_type == SUSPEND_FOR)
			{
				// process this pending event in-place and push new Action on the top of the stack
				ActionResult< Actor > result(under->m_eventResult.m_type, under->m_eventResult.m_action, under->m_eventResult.m_reason);

				// clear event result in case this change is a suspend and we later resume this action
				under->m_eventResult = TryContinue(RESULT_NONE);

				return result;
			}

			under = under->GetActionBuriedUnderMe();
		}

		return Continue();
	}

	// given the result of this Action's work, apply the result to potentially cause a state transition
	Action< Actor >* ApplyResult(Actor* me, Behavior< Actor >* behavior, ActionResult< Actor > result);

	/**
	 * The methods below do the bookkeeping of each event, propagate the activity through the hierarchy,
	 * and invoke the virtual event for each.
	 */
	ActionResult< Actor >	InvokeOnStart(Actor* me, Behavior< Actor >* behavior, Action< Actor >* priorAction, Action< Actor >* buriedUnderMeAction);
	ActionResult< Actor >	InvokeUpdate(Actor* me, Behavior< Actor >* behavior, float interval);
	void					InvokeOnEnd(Actor* me, Behavior< Actor >* behavior, Action< Actor >* nextAction);
	Action< Actor >* InvokeOnSuspend(Actor* me, Behavior< Actor >* behavior, Action< Actor >* interruptingAction);
	ActionResult< Actor >	InvokeOnResume(Actor* me, Behavior< Actor >* behavior, Action< Actor >* interruptingAction);

	/**
	 * Store the given event result, attending to priorities
	 */
	void StorePendingEventResult(const EventDesiredResult< Actor >& result, const char* eventName)
	{
		if (result.IsContinue())
		{
			return;
		}

		if (result.m_priority > m_eventResult.m_priority)
		{
			// new result is more important - destroy the replaced action
			if (result.m_priority != RESULT_NONE && m_eventResult.m_action)
			{
				delete m_eventResult.m_action;
			}

			m_eventResult = result;
		}
		else if (result.m_priority == m_eventResult.m_priority && m_eventResult.m_type == SUSTAIN)
		{
			// same priority as existing SUSTAIN, override

			// destroy the replaced action
			if (m_eventResult.m_action)
			{
				delete m_eventResult.m_action;
			}

			m_eventResult = result;
		}
		else
		{
			if (result.m_action)
			{
				// destroy the unused action
				delete result.m_action;
			}
		}
	}

	char* BuildDecoratedName(char* name, const Action< Actor >* action) const;	// recursive name outMsg for DebugString()
};


//-------------------------------------------------------------------------------------------
template < typename Actor >
Action< Actor >::Action(void)
{
	m_parent = NULL;
	m_child = NULL;
	m_buriedUnderMe = NULL;
	m_coveringMe = NULL;
	m_actor = NULL;
	m_behavior = NULL;

	m_isStarted = false;
	m_isSuspended = false;

	m_eventResult = TryContinue(RESULT_NONE);

#ifdef DEBUG_BEHAVIOR_MEMORY
	ConColorMsg(Color(255, 0, 255, 255), "%3.2f: NEW %0X\n", gpGlobals->curtime, this);
#endif
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
Action< Actor >::~Action()
{
#ifdef DEBUG_BEHAVIOR_MEMORY
	ConColorMsg(Color(255, 0, 255, 255), "%3.2f: DELETE %0X\n", gpGlobals->curtime, this);
#endif

	if (m_parent)
	{
		// if I'm my parent's active child, update parent's pointer
		if (m_parent->m_child == this)
		{
			m_parent->m_child = m_buriedUnderMe;
		}
	}

	// delete all my children.
	// our m_child pointer always points to the topmost
	// child in the stack, so work our way back thru the
	// 'buried' children and delete them.
	Action< Actor >* child, * next = NULL;
	for (child = m_child; child; child = next)
	{
		next = child->m_buriedUnderMe;
		delete child;
	}

	if (m_buriedUnderMe)
	{
		// we're going away, so my buried sibling is now on top
		m_buriedUnderMe->m_coveringMe = NULL;
	}

	// delete any actions stacked on top of me
	if (m_coveringMe)
	{
		// recursion will march down the chain
		delete m_coveringMe;
	}

	// delete any pending event result
	if (m_eventResult.m_action)
	{
		delete m_eventResult.m_action;
	}
}


template < typename Actor >
bool Action< Actor >::IsNamed(const char* name) const
{
	return strncmp(GetName(), name, 32);
}


template < typename Actor >
Actor* Action< Actor >::GetActor(void) const
{
	return m_actor;
}

template < typename Actor >
ActionResult< Actor > Action< Actor >::Continue(void) const
{
	return ActionResult< Actor >(CONTINUE, NULL, NULL);
}

template < typename Actor >
ActionResult< Actor > Action< Actor >::ChangeTo(Action< Actor >* action, const char* reason) const
{
	return ActionResult< Actor >(CHANGE_TO, action, reason);
}

template < typename Actor >
ActionResult< Actor > Action< Actor >::SuspendFor(Action< Actor >* action, const char* reason) const
{
	// clear any pending transitions requested by events, or this SuspendFor will
	// immediately be out of scope
	m_eventResult = TryContinue(RESULT_NONE);

	return ActionResult< Actor >(SUSPEND_FOR, action, reason);
}

template < typename Actor >
ActionResult< Actor > Action< Actor >::Done(const char* reason) const
{
	return ActionResult< Actor >(DONE, NULL, reason);
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
EventDesiredResult< Actor > Action< Actor >::TryContinue(EventResultPriorityType priority) const
{
	return EventDesiredResult< Actor >(CONTINUE, NULL, priority);
}

template < typename Actor >
EventDesiredResult< Actor > Action< Actor >::TryChangeTo(Action< Actor >* action, EventResultPriorityType priority, const char* reason) const
{
	return EventDesiredResult< Actor >(CHANGE_TO, action, priority, reason);
}

template < typename Actor >
EventDesiredResult< Actor > Action< Actor >::TrySuspendFor(Action< Actor >* action, EventResultPriorityType priority, const char* reason) const
{
	return EventDesiredResult< Actor >(SUSPEND_FOR, action, priority, reason);
}

template < typename Actor >
EventDesiredResult< Actor > Action< Actor >::TryDone(EventResultPriorityType priority, const char* reason /*= NULL*/) const
{
	return EventDesiredResult< Actor >(DONE, NULL, priority, reason);
}

template < typename Actor >
EventDesiredResult< Actor > Action< Actor >::TryToSustain(EventResultPriorityType priority, const char* reason /*= NULL*/) const
{
	return EventDesiredResult< Actor >(SUSTAIN, NULL, priority, reason);
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
Action< Actor >* Action< Actor >::GetActiveChildAction(void) const
{
	return m_child;
}


//-------------------------------------------------------------------------------------------
// the Action that I'm running inside of
template < typename Actor >
Action< Actor >* Action< Actor >::GetParentAction(void) const
{
	return m_parent;
}


//-------------------------------------------------------------------------------------------
/**
 * Return true if we are currently suspended for another Action
 */
template < typename Actor >
bool Action< Actor >::IsSuspended(void) const
{
	return m_isSuspended;
}


//-------------------------------------------------------------------------------------------
/**
 * Start this Action.
 * The act of calling InvokeOnStart is the edge case that 'enters' a state.
 */
template < typename Actor >
ActionResult< Actor > Action< Actor >::InvokeOnStart(Actor* me, Behavior< Actor >* behavior, Action< Actor >* priorAction, Action< Actor >* buriedUnderMeAction)
{
	// these value must be valid before invoking OnStart, in case an OnSuspend happens 
	m_isStarted = true;
	m_actor = me;
	m_behavior = behavior;

	// maintain parent/child relationship during transitions
	if (priorAction)
	{
		m_parent = priorAction->m_parent;
	}

	if (m_parent)
	{
		// child pointer of an Action always points to the ACTIVE child
		// parent pointers are set when child Actions are instantiated
		m_parent->m_child = this;
	}

	// maintain stack pointers
	m_buriedUnderMe = buriedUnderMeAction;
	if (buriedUnderMeAction)
	{
		buriedUnderMeAction->m_coveringMe = this;
	}

	// we are always on top of the stack. if our priorAction was buried, it cleared 
	// everything covering it when it ended (which happens before we start)
	m_coveringMe = NULL;

	// start the optional child action
	m_child = InitialContainedAction(me);
	if (m_child)
	{
		// define initial parent/child relationship
		m_child->m_parent = this;

		m_child = m_child->ApplyResult(me, behavior, ChangeTo(m_child, "Starting child Action"));
	}

	// start ourselves
	ActionResult< Actor > result = OnStart(me, priorAction);

	return result;
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
ActionResult< Actor > Action< Actor >::InvokeUpdate(Actor* me, Behavior< Actor >* behavior, float interval)
{
	// an explicit "out of scope" check is needed here to prevent any
	// pending events causing an out of scope action to linger
	if (IsOutOfScope())
	{
		// exit self to make this Action active and allow result to take effect on its next Update
		return Done("Out of scope");
	}

	if (!m_isStarted)
	{
		// this Action has not yet begun - start it
		return ChangeTo(this, "Starting Action");
	}

	// honor any pending event results 
	ActionResult< Actor > eventResult = ProcessPendingEvents();
	if (!eventResult.IsContinue())
	{
		return eventResult;
	}

	// update our child action first, since it has the most specific behavior
	if (m_child)
	{
		m_child = m_child->ApplyResult(me, behavior, m_child->InvokeUpdate(me, behavior, interval));
	}

	// update ourselves
	ActionResult< Actor > result = Update(me, interval);

	return result;
}


//-------------------------------------------------------------------------------------------
/**
 * This method calls the virtual OnEnd() method for the Action, its children, and Actions
 * stacked on top of it.
 * It does NOT delete resources, or disturb pointer relationships, because this Action
 * needs to remain valid for a short while as an argument to OnStart(), OnSuspend(), etc for
 * the next Action.
 * The destructor for the Action frees memory for this Action, its children, etc.
 */
template < typename Actor >
void Action< Actor >::InvokeOnEnd(Actor* me, Behavior< Actor >* behavior, Action< Actor >* nextAction)
{
	if (!m_isStarted)
	{
		// we are not started (or never were)
		return;
	}

	// we are no longer started
	m_isStarted = false;

	// tell child Action(s) to leave (but don't disturb the list itself)
	Action< Actor >* child, * next = NULL;
	for (child = m_child; child; child = next)
	{
		next = child->m_buriedUnderMe;
		child->InvokeOnEnd(me, behavior, nextAction);
	}

	// leave ourself
	OnEnd(me, nextAction);

	// leave any Actions stacked on top of me
	if (m_coveringMe)
	{
		m_coveringMe->InvokeOnEnd(me, behavior, nextAction);
	}
}


//-------------------------------------------------------------------------------------------
/**
 * Just invoke OnSuspend - when the interrupting Action is started it will
 * update our buried/covered pointers.
 * OnSuspend may cause this Action to exit.
 */
template < typename Actor >
Action< Actor >* Action< Actor >::InvokeOnSuspend(Actor* me, Behavior< Actor >* behavior, Action< Actor >* interruptingAction)
{

	// suspend child Action
	if (m_child)
	{
		m_child = m_child->InvokeOnSuspend(me, behavior, interruptingAction);
	}

	// suspend ourselves
	m_isSuspended = true;
	ActionResult< Actor > result = OnSuspend(me, interruptingAction);

	if (result.IsDone())
	{
		// we want to be replaced instead of suspended
		InvokeOnEnd(me, behavior, NULL);

		Action< Actor >* buried = GetActionBuriedUnderMe();

		delete this;

		// new Action on top of the stack
		return buried;
	}

	// we are still on top of the stack at this moment
	return this;
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
ActionResult< Actor > Action< Actor >::InvokeOnResume(Actor* me, Behavior< Actor >* behavior, Action< Actor >* interruptingAction)
{
	if (!m_isSuspended)
	{
		// we were never suspended
		return Continue();
	}

	if (m_eventResult.IsRequestingChange())
	{
		// this Action is not actually being Resumed, because a change
		// is already pending from a prior event
		return Continue();
	}

	// resume ourselves
	m_isSuspended = false;
	m_coveringMe = NULL;

	if (m_parent)
	{
		// we are once again our parent's active child
		m_parent->m_child = this;
	}

	// resume child Action
	if (m_child)
	{
		m_child = m_child->ApplyResult(me, behavior, m_child->InvokeOnResume(me, behavior, interruptingAction));
	}

	// actually resume ourselves
	ActionResult< Actor > result = OnResume(me, interruptingAction);

	return result;
}


//-------------------------------------------------------------------------------------------
/**
 * Given the result of this Action's work, apply the result to potentially create a new Action
 */
template < typename Actor >
Action< Actor >* Action< Actor >::ApplyResult(Actor* me, Behavior< Actor >* behavior, ActionResult< Actor > result)
{
	Action< Actor >* newAction = result.m_action;

	switch (result.m_type)
	{
		//-----------------------------------------------------------------------------------------------------
		// transition to new Action
	case CHANGE_TO:
	{
		if (newAction == NULL)
		{
			return this;
		}

		// we are done
		this->InvokeOnEnd(me, behavior, newAction);

		// start the new Action
		ActionResult< Actor > startResult = newAction->InvokeOnStart(me, behavior, this, this->m_buriedUnderMe);

		// discard ended action
		if (this != newAction)
		{
			delete this;
		}

		// apply result of starting the Action
		return newAction->ApplyResult(me, behavior, startResult);
	}

	//-----------------------------------------------------------------------------------------------------
	// temporarily suspend ourselves for the newAction, covering it on the stack
	case SUSPEND_FOR:
	{
		// interrupting Action always goes on the TOP of the stack - find it
		Action< Actor >* topAction = this;
		while (topAction->m_coveringMe)
		{
			topAction = topAction->m_coveringMe;
		}

		// suspend the Action we just covered up				
		topAction = topAction->InvokeOnSuspend(me, behavior, newAction);

		// begin the interrupting Action.
		ActionResult< Actor > startResult = newAction->InvokeOnStart(me, behavior, topAction, topAction);

		return newAction->ApplyResult(me, behavior, startResult);
	}

	//-----------------------------------------------------------------------------------------------------
	case DONE:
	{
		// resume buried action
		Action< Actor >* resumedAction = this->m_buriedUnderMe;

		// we are finished
		this->InvokeOnEnd(me, behavior, resumedAction);

		if (resumedAction == NULL)
		{
			// all Actions complete
			delete this;
			return NULL;
		}

		// resume uncovered action
		ActionResult< Actor > resumeResult = resumedAction->InvokeOnResume(me, behavior, this);

		// discard ended action
		delete this;

		// apply result of OnResume()
		return resumedAction->ApplyResult(me, behavior, resumeResult);
	}

	case CONTINUE:
	case SUSTAIN:
	default:
	{
		// no change, continue the current action next frame
		return this;
	}
	}
}


//-------------------------------------------------------------------------------------------
/**
 * Propagate events to sub actions
 */
template < typename Actor >
INextBotEventResponder* Action< Actor >::FirstContainedResponder(void) const
{
	return GetActiveChildAction();
}

template < typename Actor >
INextBotEventResponder* Action< Actor >::NextContainedResponder(INextBotEventResponder* current) const
{
	return NULL;
}


//-------------------------------------------------------------------------------------------
/**
 * Return a temporary string describing the current action stack for debugging
 */
template < typename Actor >
const char* Action< Actor >::DebugString(void) const
{
	static char str[256];

	str[0] = '\000';

	// find root
	const Action< Actor >* root = this;
	while (root->m_parent)
	{
		root = root->m_parent;
	}

	return BuildDecoratedName(str, root);
}


//-------------------------------------------------------------------------------------------
template < typename Actor >
char* Action< Actor >::BuildDecoratedName(char* name, const Action< Actor >* action) const
{
	const int fudge = 256;

	// add the name of the given action
	Q_strcat(name, action->GetName(), fudge);

	// add any contained actions
	const Action< Actor >* child = action->GetActiveChildAction();
	if (child)
	{
		Q_strcat(name, "( ", fudge);
		BuildDecoratedName(name, child);
		Q_strcat(name, " )", fudge);
	}

	// append buried actions
	const Action< Actor >* buried = action->GetActionBuriedUnderMe();
	if (buried)
	{
		Q_strcat(name, "<<", fudge);
		BuildDecoratedName(name, buried);
	}

	return name;
}


//-------------------------------------------------------------------------------------------
/**
 * Return a temporary string showing the full lineage of this one action
 */
template < typename Actor >
const char* Action< Actor >::GetFullName(void) const
{
	const int fudge = 256;
	static char str[fudge];

	str[0] = '\000';

	const int maxStack = 64;
	const char* nameStack[maxStack];
	int stackIndex = 0;

	for (const Action< Actor >* action = this;
		stackIndex < maxStack && action;
		action = action->m_parent)
	{
		nameStack[stackIndex++] = action->GetName();
	}

	// assemble name
	int i;
	for (i = stackIndex - 1; i; --i)
	{
		Q_strcat(str, nameStack[i], fudge);
		Q_strcat(str, "/", fudge);
	}
	
	Q_strcat(str, nameStack[0], fudge);

	/*
	for( i = 0; i < stackIndex-1; ++i )
	{
		Q_strcat( str, " )", fudge );
	}
	*/

	return str;
}


#endif // _BEHAVIOR_ENGINE_H_




