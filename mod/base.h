#pragma once
#include<string>

class Player;
class Actor;
class Vec3;
//class AutomaticID<Dimension,int>;
class Dimension;
#include"minecraft/core/types.h"
using std::string;
void reg_attack(void* a);
void reg_pickup(void* a);
void reg_useitem(void* a);
void reg_useitemon(void* a);
void reg_build(void* a);
void reg_destroy(void* a);
void sendText(Player* a,string ct);
void TeleportA(Actor& a,Vec3 b,AutomaticID<Dimension,int> c);
Player* getplayer_byname(const string& name);
Player* getplayer_byname2(const string& name);
void KillActor(Actor* a);
void sendText2(Player* a,string ct);
MCRESULT runcmd(const string& a);

#define ARGSZ(b) if(a.size()<b){outp.error("check your arg");return;}

