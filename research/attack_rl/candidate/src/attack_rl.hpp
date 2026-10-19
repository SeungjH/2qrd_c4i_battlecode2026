#pragma once
#include "bot.hpp"
#include "rl_weights.hpp"
namespace battlecode {
struct AttackRL {
    static void disable(Bot& b){b.rlAttackEnabled=false;}
    static array<double,9> forward(const array<double,522>& x) {
        array<double,32> h{};array<double,9> y{};
        for(int j=0;j<32;j++){double s=rlmodel::b1[j];for(int k=0;k<522;k++)s+=rlmodel::w1[j*522+k]*x[k];h[j]=tanh(s);}
        for(int a=0;a<9;a++){y[a]=rlmodel::b2[a];for(int j=0;j<32;j++)y[a]+=rlmodel::w2[a*32+j]*h[j];}
        return y;
    }
static array<double,522> observation(Bot& b) {
    array<double,522>x{};
    for(int j=0;j<49;j++) {
        const Tile&t=b.tiles[j];int k=j*10;
        x[k]=t.pearl;x[k+1]=t.id==b.id;x[k+2]=t.id>=0&&t.team==b.team&&t.id!=b.id;
        x[k+3]=t.id>=0&&t.team!=b.team;x[k+4]=t.head&&t.team==b.team&&t.id!=b.id;
        x[k+5]=t.head&&t.team!=b.team;
        for(int d=0;d<4;d++)x[k+6+d]=b.edge(j,d)=="w"?1:b.edge(j,d)=="."?0:-1;
    }
    int k=490;
    auto delta=[](int a,int b,int size){int d=b-a;if(d>size/2)d-=size;if(d<-size/2)d+=size;return clamp(d/8.0,-2.0,2.0);};
    x[k++]=b.length/12.0;x[k++]=b.roundNo/500.0;x[k++]=b.width/64.0;x[k++]=b.height/64.0;
    for(int d=0;d<4;d++)x[k++]=b.facing==d;
    x[k++]=delta(b.tiles[24].p.x,b.rlAttackEnemy.x,b.width);x[k++]=delta(b.tiles[24].p.y,b.rlAttackEnemy.y,b.height);
    x[k++]=min(b.roundNo-b.rlAttackSeen,10)/10.0;x[k++]=b.rlAttackSlot/2.0;
    x[k++]=b.rlAttackPeers.size()/2.0;x[k++]=min(b.roundNo-b.rlAttackStarted,40)/40.0;
    x[k++]=min(b.visits[b.code(b.tiles[24].p)],10)/10.0;x[k++]=b.terrainDegree(b.code(b.tiles[24].p))/4.0;
    for(int peerIndex=0;peerIndex<2;peerIndex++) {
        if(peerIndex>=int(b.rlAttackPeers.size())){k+=8;continue;}
        int owner=b.rlAttackPeers[peerIndex],at=b.visibleHead(owner);auto it=b.allies.find(owner);Pos p;
        if(at>=0)p=b.tiles[at].p;else if(it!=b.allies.end())p=it->second.head;else{k+=8;continue;}
        x[k++]=1;x[k++]=delta(b.tiles[24].p.x,p.x,b.width);x[k++]=delta(b.tiles[24].p.y,p.y,b.height);
        int count=0;for(const Tile&t:b.tiles)count+=t.id==owner;
        x[k++]=(it!=b.allies.end()?it->second.len:count)/12.0;
        x[k++]=it!=b.allies.end()?min(b.roundNo-it->second.stamp,10)/10.0:0;
        int slot=int(find(b.rlAttackRoster.begin(),b.rlAttackRoster.end(),owner)-b.rlAttackRoster.begin());
        for(int d=0;d<3;d++)x[k++]=slot==d;
    }
    return x;
}
static bool hasExit(Bot& b,int direction,int steps=1) {
    int at=24;vector<Pos> trail=b.body;
    bool complete=trail.size()==size_t(b.length)&&none_of(trail.begin(),trail.end(),[](Pos p){return p.x<0;});
    if(trail.size()!=size_t(b.length)||any_of(trail.begin(),trail.end(),[](Pos p){return p.x<0;})) {
        trail.clear();for(const Tile&t:b.tiles)if(t.id==b.id)trail.push_back(t.p);
    }
    for(int step=0;step<steps;step++) {
        int to=b.nextTile(at,direction,false);if(to<0)return false;
        if(b.tiles[to].head&&b.tiles[to].team!=b.team&&b.tiles[to].id>=0)return steps==1;
        trail.insert(trail.begin(),b.tiles[to].p);if(complete&&!b.tiles[to].pearl&&!trail.empty())trail.pop_back();at=to;
    }
    array<bool,49>blocked{};
    for(int j=0;j<49;j++)blocked[j]=b.tiles[j].id>=0&&b.tiles[j].id!=b.id;
    for(Pos p:trail){int j=b.local(p);if(j>=0)blocked[j]=true;}
    bool exit=false;
    for(int d=0;d<4;d++)if(b.edge(at,d)=="."){
        int to=b.nextTile(at,d,false);if(to<0||!blocked[to])exit=true;
    }
    if(!exit)return false;
    Region room=b.region(at,blocked,false);
    return room.frontier||room.area>=min(b.length+2,14);
}

    static bool choose(Bot& b,ActionPlan& plan) {
        if(!b.rlAttackEnabled || !b.rlAttackFresh || plan.splitSize || plan.reason!="HUNT_BLOCK" ||
           b.role!=4 || b.officialQueen() || b.protectedLong() || b.roundNo>=350 ||
           b.length<2 || b.length>4 || b.visibleHead(b.huntEnemy)<0 ||
           b.rlAttackSlot>=3 || b.rlAttackPeers.empty())return false;
        if(any_of(b.rlAttackPeers.begin(),b.rlAttackPeers.end(),[](int owner){return owner<=1;}))return false;
        auto logits=forward(observation(b));int best=-1;double value=-1e100;
        for(int d=0;d<4;d++){
            if(b.edge(24,d)!=".")continue;
            int to=b.nextTile(24,d,false);if(to<0)continue;
            const Tile& t=b.tiles[to];
            if(t.id>=0 && !(t.head && t.team!=b.team))continue;
            if(!hasExit(b,d))continue;
            if(logits[d]>value){best=d;value=logits[d];}
        }
        if(best<0)return false;
        int previous=plan.chosen.dir;
        plan.chosen=Move{};plan.chosen.dir=best;plan.chosen.to=b.nextTile(24,best,false);
        plan.reason="RL_ATTACK";
        cout<<"LOG RL_V14_ATTACK "<<b.id<<' '<<b.roundNo<<' '<<best<<' '<<b.huntEnemy<<' '<<b.rlAttackSlot<<' '<<previous<<'\n';
        return true;
    }
};
}
