#pragma once
#include "state.hpp"

namespace battlecode {

// One dragon's controller. Mutable memory is owned here, never at file scope.
// Data-only private inheritance keeps the existing field spelling during a
// behavior-preserving migration; it does not create polymorphic strategies.
class Bot : private BotState {
public:
    Bot() = default;
    Bot(const Bot&) = delete;
    Bot& operator=(const Bot&) = delete;
    int run();

private:
    friend struct BotTestAccess;
    // geometry
    static int wrap(int n,int m);
    static int code(Pos p);
    static int dirOf(char c);
    int local(Pos p);
    int distance(Pos a,Pos b);
    int homeDistance(Pos p,int home);
    static Pos decodePos(int k);
    bool validKey(int k);
    int visibleHead(int owner);
    bool visibleAlly(int owner);
    bool freshAlly(int owner);

    // communication
    unsigned long long encodePacket(Packet p);
    static int packetPriority(int type);
    void sendPacket(int type,unsigned payload,int stamp=-1,bool /*urgent*/=false);
    bool feedTargetFresh();
    void selectFeedTarget();
    void rememberAllyReport(int owner,int len,Pos head,int stamp);
    bool reservedPearl(int key);
    bool exclusivePearl(int key);
    void rememberResource(int key,int score,int stamp,bool fountain);
    void addWarning(int entrance,int d,int stamp,bool permanent,bool localProof=false);
    bool avoided(int k,int d);
    void receiveRadio();
    void emitRadio();

    // world
    const string& edge(int at,int d);
    void remember(int portalId,bool horizontal,Pos p);
    int portalDestination(int from,int d,int portalId);
    bool knownPortalTrap(int destination);
    bool portalReady(int portalId);
    int nextTile(int at,int d,bool respectCooldown=true);
    void syncBody();
    void rememberTerrain();
    int terrainDegree(int k);
    void recordMove(const Move &m);
    void observePortalArrival();

    // movement
    array<bool,49> afterMove(int to);
    Region region(int start,const array<bool,49>& blocked,bool portals=true);
    double headRisk(int target);
    double separation(int target);
    bool obstructsQueen(int target);
    bool longContinuation(const vector<int>& trail,const array<bool,4096>& others, array<bool,49> eaten,int depth,int &budget);
    bool sustainableLongMove(int to);
    Move evaluate(int d,int to);
    Move distantPortal(int d);
    vector<Move> legalMoves();
    bool safeMove(const Move &m);
    bool promote(vector<Move>& moves,int d);

    // navigation
    bool guideOutOfLoop(vector<Move>& moves);
    bool profitablePocket(const Move &m);
    bool profitablePocketMove(vector<Move>& moves);
    int routeDirection(const vector<Move>& moves,int destination);
    int routeToMeeting(const vector<Move>& moves,int destination);
    bool pearlPathSafe(const vector<int>& path);
    bool guideToPearl(vector<Move>& moves);
    bool guideToResource(vector<Move>& moves);
    void updateCorridor(const Move &m);
    bool guideCourier(vector<Move>& moves);
    void prepareEscapeRadio();

    // feeding
    vector<int> visibleBody(int owner,int expected);
    Collection collectionPlan(int donor,int receiver,int receiverLength);
    int deliberateDeathDirection();
    bool betterRank(int owner,int len);
    bool approachAlly(vector<Move>& moves,int owner);
    bool eligibleDonor(int donorId,int donorLength);
    bool donationCandidate();
    bool circleMeeting(vector<Move>& moves,int meeting);
    bool donorHasTurn();
    bool queenAssembly(vector<Move>& moves);
    bool coordinateDonation(vector<Move>& moves,Move &chosen,string &reason);
    int donorSprint(const Move &first,const string &reason);

    // combat
    void endHunt();
    bool coordinateHunt(vector<Move>& moves);
    bool newbornAmbush();
    int favorableHeadTrade();
    int tailAmbushEnemy(const vector<Move>& moves);

    // survival
    bool quietEndgame();
    bool officialQueen();
    bool recentIncome(int window=12);
    bool protectedLong();
    bool trappedAllyAtTail();
    bool cutCanReleaseParent();
    bool physicallyTrapped();
    bool queenTailReleaseSplit();
    int queenReleaseSplitSize();
    bool relieveQueenLane(ActionPlan &plan);
    int giftExpiry();
    bool newbornQueenTailGift();
    bool retainedQueenCanEscape();
    bool reverseRescue(const vector<Move>& moves);
    bool pocketEscapeSplit(const vector<Move>& moves);
    int emergencyPortalDirection();
    bool routineSplit(const vector<Move>& moves);
    int branchChildDirection(const vector<Move>& moves);

    // protocol
    bool readTurn();

    // Turn orchestration; precedence is intentionally identical to v13.
    bool initialize();
    bool handleTerminalAction(bool &recoveredQueen);
    void updateTurnMemory();
    ActionPlan planAction();
    void chooseFallback(ActionPlan &plan);
    void executeAction(ActionPlan &plan);
    void finishTurn(const ActionPlan &plan);
    void playTurn();
};

} // namespace battlecode
