// NextBotInterface.h
// Interface for NextBot
// Author: Michael Booth, May 2006
//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef _NEXT_BOT_INTERFACE_H_
#define _NEXT_BOT_INTERFACE_H_

#include "NextBotComponentInterface.h"
#include "NextBotIntentionInterface.h"
#include "NextBotDebug.h"

class CBaseCombatCharacter;
class PathFollower;

//----------------------------------------------------------------------------------------------------------------
/**
 * A general purpose filter interface for various bot systems
 */
class INextBotFilter
{
public:
	virtual bool IsSelected( const CBaseEntity *candidate ) const = 0;			// return true if this entity passes the filter
};


//----------------------------------------------------------------------------------------------------------------
class INextBot : public INextBotEventResponder
{
public:
	INextBot( void );
	virtual ~INextBot() = 0;

	virtual void Reset( void ) = 0;										// (EXTEND) reset to initial state
	virtual void Update( void ) = 0;									// (EXTEND) update internal state
	virtual void Upkeep( void ) = 0;									// (EXTEND) lightweight update guaranteed to occur every server tick

	virtual bool IsRemovedOnReset( void ) const { return true; }	// remove this bot when the NextBot manager calls Reset

	virtual CBaseCombatCharacter *GetEntity( void ) const	= 0;
	virtual class NextBotCombatCharacter *GetNextBotCombatCharacter( void ) const	{ return NULL; }

	// interfaces are never NULL - return base no-op interfaces at a minimum
	virtual void*		GetLocomotionInterface( void ) const = 0;
	virtual void*		GetBodyInterface( void ) const = 0;
	virtual IIntention*	GetIntentionInterface( void ) const = 0;
	virtual void*		GetVisionInterface( void ) const = 0;

	/**
	 * Attempt to change the bot's position. Return true if successful.
	 */
	virtual bool SetPosition( const Vector &pos ) = 0;
	virtual const Vector &GetPosition( void ) const = 0;				// get the global position of the bot

	/**
	 * Friend/enemy/neutral queries
	 */
	virtual bool IsEnemy( const CBaseEntity *them ) const = 0;			// return true if given entity is our enemy
	virtual bool IsFriend( const CBaseEntity *them ) const = 0;			// return true if given entity is our friend
	virtual bool IsSelf( const CBaseEntity *them ) const = 0;			// return true if 'them' is actually me

	/**
	 * Can we climb onto this entity?
	 */	
	virtual bool IsAbleToClimbOnto( const CBaseEntity *object ) const = 0;

	/**
	 * Can we break this entity?
	 */	
	virtual bool IsAbleToBreak( const CBaseEntity *object ) const = 0;

	/**
	 * Sometimes we want to pass through other NextBots. OnContact() will always
	 * be invoked, but collision resolution can be skipped if this
	 * method returns false.
	 */
	virtual bool IsAbleToBlockMovementOf( const INextBot *botInMotion ) const	{ return true; }

	/**
	 * Should we ever care about noticing physical contact with this entity?
	 */
	virtual bool ShouldTouch( const CBaseEntity *object ) const		{ return true; }

	/**
	 * This immobile system is used to track the global state of "am I actually moving or not".
	 * The OnStuck() event is only emitted when following a path, and paths can be recomputed, etc.
	 */
	virtual bool IsImmobile( void ) const = 0;					// return true if we haven't moved in awhile
	virtual float GetImmobileDuration( void ) const = 0;		// how long have we been immobile
	virtual void ClearImmobileStatus( void ) = 0;		
	virtual float GetImmobileSpeedThreshold( void ) const = 0;	// return units/second below which this actor is considered "immobile"

	/**
	 * Get the last PathFollower we followed. This method gives other interfaces a
	 * single accessor to the most recent Path being followed by the myriad of 
	 * different PathFollowers used in the various behaviors the bot may be doing.
	 */
	virtual const PathFollower *GetCurrentPath( void ) const = 0;
	virtual void SetCurrentPath( const PathFollower *path ) = 0;
	virtual void NotifyPathDestruction( const PathFollower *path ) = 0;		// this PathFollower is going away, which may or may not be ours

	// between distance utility methods
	virtual bool IsRangeLessThan( CBaseEntity *subject, float range ) const = 0;
	virtual bool IsRangeLessThan( const Vector &pos, float range ) const = 0;
	virtual bool IsRangeGreaterThan( CBaseEntity *subject, float range ) const = 0;
	virtual bool IsRangeGreaterThan( const Vector &pos, float range ) const = 0;
	virtual float GetRangeTo( CBaseEntity *subject ) const = 0;
	virtual float GetRangeTo( const Vector &pos ) const = 0;
	virtual float GetRangeSquaredTo( CBaseEntity *subject ) const = 0;
	virtual float GetRangeSquaredTo( const Vector &pos ) const = 0;

	// event propagation
	virtual INextBotEventResponder *FirstContainedResponder( void ) const = 0;
	virtual INextBotEventResponder *NextContainedResponder( INextBotEventResponder *current ) const = 0;

	virtual bool IsDebugging( unsigned int type ) const = 0;		// return true if this bot is debugging any of the given types
	virtual const char *GetDebugIdentifier( void ) const = 0;		// return the name of this bot for debugging purposes
	virtual bool IsDebugFilterMatch( const char *name ) const = 0;	// return true if we match the given debug symbol
	virtual void DisplayDebugText( const char *text ) const = 0;	// show a line of text on the bot in the world

	enum {
		MAX_NEXTBOT_DEBUG_HISTORY = 100,
		MAX_NEXTBOT_DEBUG_LINE_LENGTH = 256,
	};
	struct NextBotDebugLineType
	{
		NextBotDebugType debugType;
		char data[ MAX_NEXTBOT_DEBUG_LINE_LENGTH ];
	};


private:
	friend class INextBotComponent;
	INextBotComponent *m_componentList;						// the first component

	const PathFollower *m_currentPath;						// the path we most recently followed

	int m_id;
	bool m_bFlaggedForUpdate;
	int m_tickLastUpdate;

	unsigned int m_debugType;
	mutable int m_debugDisplayLine;

	Vector m_immobileAnchor;
	//CountdownTimer m_immobileCheckTimer;
	//IntervalTimer m_immobileTimer;

	mutable void 		*m_baseLocomotion;
	mutable void		*m_baseBody;
	mutable IIntention	*m_baseIntention;
	mutable void		*m_baseVision;
	//mutable IAttention	*m_baseAttention;

	CUtlVector< NextBotDebugLineType * > m_debugHistory;
};


#endif // _NEXT_BOT_INTERFACE_H_