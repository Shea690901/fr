/* Baldrick: Added stat modifier for extreme strength */

inherit "/std/guild";

int query_skill_cost(string skill)
{
 return 0;  /* Need a list of possible skills first.  Awaiting Dank */
}

void setup() 
  {
  set_name("warrior");
  set_short("warrior");
  set_long(
     "Warriors are the best fighters, and can take an enourous amount "
     "of damage, but have no magical skill at all.  Warriors can use all "
     "weapons and armour available on Magisk Jord.  "
     "The Warriors guild teaches one principle of survival: beating "
     "people to a pulp!  It gives you skills that relate to finding "
     "out how good a weapon or armour is. Warriors hack and slash "
     "their way through life, and do not appear to be much good at "
     "anything else.\n");
  reset_get();
  add_guild_command("judge", 1);
  add_guild_command("do_ambush",1);
  add_guild_command("fix", 1);
  add_guild_command("smash",1);
  add_guild_command("wrestle",1);
  add_guild_command("parry",1);
  add_guild_command("disarm",1);
  add_guild_command("rush",1);
  set_main_skill("str");
}

int query_advance_cost() { return  500; }
int query_xp_cost()      { return 2000; }
int query_dice()         { return 10;   }
int query_legal_race(string race) { return 1; }
int query_thac0_step()   { return 2;    }
int query_extr_str()    { return 1;    }
int query_bing()    { return 1;    }

/*
void set_gp(object ob)
{ 
  ob->set_max_gp(level+(((int)ob->query_str()+(int)ob->query_con())/2));
}

void set_hp(object ob)
{ 
 int lvl;
 lvl = (int)ob->query_level();
 ob->set_max_hp(query_dice()*lvl);
 }
*/
 
void init()
{
 this_player()->adjust_tmp_tohit_bon(this_player()->query_tmp_tohit_bon());
 this_player()->adjust_tmp_damage_bon(this_player()->query_tmp_damage_bon());
 this_player()->adjust_tmp_ac_bon(this_player()->query_tmp_ac_bon());
 ::init();
}