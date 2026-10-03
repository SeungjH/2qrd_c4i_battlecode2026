#pragma once
#include "types.hpp"

namespace battlecode {

// The current engine observation. Foreign occupancy is replaced every turn.
struct ObservationState {
    array<Tile,49> tiles{};
    string he[8][7],ve[7][8];
    array<int,4096> localAt{};
    int width=0,height=0,id=0,unitLimit=0,roundNo=0,length=0,units=0,facing=0;
    char team=0;
};

// Only observed terrain, portals and resource history are retained.
struct WorldMemory {
    map<int,vector<Portal>> portalPairs;
    array<int,4096> visits{},lastVisit{},lastSeen{};
    array<array<int,4>,4096> terrain{},portalAt{};
    vector<pair<int,int>> knownPortalEdges;
    array<int,4096> pearlSeen{};
    map<int,int> portalLastUsed,sharedPortalExit,portalRouteLastSent;
    int lastPortalEntry=-1,lastPortalDir=-1,lastPortalId=-1,lastPortalRound=-100;
    map<int,Resource> resources;
    array<int,4096> spawnOneRound{},harvestRound{},harvestCount{},pearlCooldown{};
    int lastEnemySeen=-100;
};

struct NavigationMemory {
    vector<Pos> body; // Head to tail; unknown initial segments have x=-1.
    bool discardTrappedHead=false;
    int bearing=-1,hand=1,turnsAlive=0,lastSplit=-100,portalUses=0;
    deque<int> recentHeads,foodRounds;
    int goal=-1,goalSince=0,navigateUntil=-1,lastNewTurn=0;
    bool navigating=false;
    int resourceGoal=-1,escapeLockUntil=-1,pearlSearchBudget=256;
    int corridorEntry=-1,corridorDir=0,courierEntry=-1,courierUntil=-1;
    int courierDir=0,courierStamp=0;
    bool courierPermanent=false;
    int reverseChildUntil=-1;
};

struct RadioMemory {
    vector<unsigned long long> inbox;
    vector<Packet> received;
    map<int,Ally> allies;
    map<int,Warning> warnings;
    unsigned warningSequence=0;
    Packet outgoing;
    bool hasOutgoing=false;
    int role=0;
};

struct FeedingMemory {
    int donationTarget=-1,donationTargetSince=-1,offerStamp=-1,receiveDonor=-1,receiveUntil=-1;
    vector<int> receivePearls;
    bool receiveConfirmed=false;
    int receiveStarted=-1,receiveMeeting=-1;
    map<int,pair<int,int>> reservedFood; // tile -> receiver, expiry
    map<int,Ally> allyReports;
    Packet retiredQueen;
    int retiredUntil=-1;
    int feedTargetId=-1,feedTargetLength=0,feedTargetStamp=-100,queenLastSent=-100;
    Pos feedTargetHead;
    int officialQueenId=-1,officialQueenDeath=-1;
    int servingDonor=-1,servingUntil=-1;
    int previousEntry=-1,previousDirection=-1;
    map<int,int> queenBadEdges;
    int assemblyCenter=-1,feedTargetSince=-1,spawnHome=-1;
    map<int,pair<int,int>> rallyReports; // owner -> coordinate, original stamp
    int donorTravelOwner=-1,donorBestDistance=1000,donorProgress=-1,donorCooldown=-1;
};

struct CombatMemory {
    int huntEnemy=-1,huntSince=-1,huntProgress=-1,huntDistance=1000,huntExits=4,huntCooldown=-1;
    deque<int> huntHeads;
    int attackChildEnemy=-1,attackChildUntil=-1;
    bool attackSplitRadio=false;
};

// Data-only grouping. Bot privately inherits this aggregate, so production users
// cannot access mutable state and separate Bot instances never share memory.
struct BotState : ObservationState,WorldMemory,NavigationMemory,RadioMemory,FeedingMemory,CombatMemory {};

} // namespace battlecode
