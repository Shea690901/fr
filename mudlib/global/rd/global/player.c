// The Realms of the Dragon Player Object
// Adopted from the Discworld baselib 6/12/93
// Various changes deleted due to the scroll
// 1/27/95 Asmodean     Added Chrisys prototype cmd system, using external files
// 03 Mar 95 Morgoth    made commands external, commented out old commands:
//                      glance, look_me, setmin, setmout, setmmin, setmmout,
//                      toggle_wimpy, sheet, encumbrance_by_ish, score, kill, stop,
//                      consider, bury, do_su
// 23 Mar 95 Begosh     external do_teach, do_learn commands
// ?? Mar 95 Baldrick   commented out reset_align_title (external?)
// 01 Jun 95 Laggard    removed old command code and comments (as above).
//                      use query_ and set_ (gp,hp,xp,wimpy).
// 15 Jun 95 Laggard    revised command priorities, now in commands.h
//                      move properties to new property.h
// ?? Jun 95 Dyraen     functions to see when someone is mailing or posting.
// 20 Jun 95 Laggard    reduced heart_beat branching and calculations.
//                      moved (gp,hp,headache) to health; use wimpy_hp.
// 30 Jun 95 Laggard    look and glance (renamed x_) are back (/global/vision),
//                      moved examine, brief_verbose, and query_verbose, too;
//                      better speed than external, access to this_object().
//                      eliminated duplicate msg's (also in /std/living/living.c)
//                      monitor occurs more often (every hit).
//                      social points counters without remaindering.
// 04 Jul 95 Laggard    use new drunken_heart_beat instead of drunk_heart_beat
//                      (/std/living/living instead of /global/drunk).
//                      setup new dead and passed_out tests.
//                      re-ordered heart_beat.
//                      monitor occurs on gp loss, too.
//                      removed old array and variable references.
// 09 Jul 95 Laggard    use metabolism intox_level.
//                      renamed quit to x_quit, to void all the shadows.
// 13 Jul 95 Dyraen     Modified query_start_pos() to return
//                      creators to their workrooms if possible.
// 09 Jul 95 Laggard    log look_me prior to deletion.
// 29 Aug 95 Dyraen     Made new characters start with 500/500hp.
// 20 Sep 95 Dyraen     Added a retire flag, so we can see if someones
//                      retired.
// 04 Oct 95 Dyraen     Added function query_sites(), returns a mapping
//                      of all the sites a player has logged in from.

#include "library.h"
#include "tune.h"
#include "login_handler.h"
#include "drinks.h"
#include "weather.h"
#include "log.h"
#include "cmd.h"
#include "commands.h"

inherit "/global/line_ed";
inherit "/global/auto_load";
inherit "/global/log";
inherit "/global/spells";
inherit "/global/help";
inherit "/global/finger";
inherit "/std/living/living";
inherit "/std/living/handle";
inherit "/global/events";
inherit "/global/stats-rear";
inherit "/global/psoul";
inherit "/global/guild-race";
//inherit "/global/drunk";
inherit "/global/last";
inherit "/global/more_file";
inherit "/global/path";
inherit "/global/environ";
inherit "/global/vision";

#include <post.h>
#include "money.h"
#include "mail.h"
#include "player.h"
#include "property.h"

#define START_POS "/d/lirath/tavern"
#define STD_RACE "/std/races/human"
#define BRIGHT_LIGHT 60
#define MIN_LEVEL 1
#define MIN_AGE 1800  /* seconds */

string cap_name;
mapping sites;
string *auto_load, last_on_from, last_pos;
mixed *money_array;
string title, al_title;
int time_on,max_deaths,monitor,refresh;
int save_counter, i, rank;
int invis, *saved_co_ords, ed_setup, start_time, hblock;
int creator, app_creator, deaths, level, last_log_on, alignment;
mapping env_var;

static int hb_sp_max, hb_sp;
static int hb_hp_saved;
static int hb_gp_saved;
static int mailing, posting, retired;
static object snoopee;

static mapping tmp_channels = ([ ]);
mapping channels = ([ ]);

void start_player();
void set_title(string str);
void public_commands();
int save_me();
int save();
void set_desc(string str);
string query_title();
int query_creator();
void set_name(string str);
int check_dark(int light);

// weird command found buried in source, ready for deletion {Laggard}
void olavolav() { ignored = ({ }); }

mapping query_channels() { return tmp_channels + channels; }

int add_t_channel( string str, string val )
{
    if( !"/secure/master"->high_programmer( geteuid( previous_object() ) ) )
        return 0;
    if( tmp_channels[str] ) tmp_channels[str] = val;
    else tmp_channels += ([ str:val ]);
    return 1;
}

int add_channel( string str, string val )
{
// Added to make sure and to make a complete check on this channel thingie
// Ie I need to know the previous object realy really sure now _bBegosh
    if( !"/secure/master"->high_programmer( geteuid( previous_object() ) ) )
        return 0;
    if( channels[str] ) channels[str] = val;
    else channels += ([ str:val ]);
    return 1;
}

int remove_t_channel( string str )
{
    if( !"/secure/master"->high_programmer( geteuid( previous_object() ) ) )
        return 0;
    map_delete( tmp_channels, str );
    return 1;
}

int remove_channel( string str )
{
    if( !"/secure/master"->high_programmer( geteuid( previous_object() ) ) )
        return 0;
    map_delete( channels, str );
    return 1;
}


void create() {
  int *i,j,k;

  living::create();
  events::create();
  psoul::create();
  line_ed::create();
/*
if (name) return;
 //added by Asmodean to Stop the idiot.
Commented out Asmo's change, because it was screwing things up.
Guests had no race object, new players couldn't look at object.
Taking out that line fixed it.  --Skaflock  */
  add_property("determinate", "");
  remove_property("new");
// following line removed so guests dont have to listen
// to chat. - Dyraen
//  add_property("chan_chat",1);
  spells = ({ });
  money_array = ({ });
  time_on = time();
  save_counter = 0;
  start_time=time();
  seteuid("PLAYER");
  Str = 13;
  Dex = 13;
  Int = 13;
  Con = 13;
  Wis = 13;
  refresh = 4;
  max_social_points = 50;
  max_deaths = 7;
  desc = 0;
  add_attack("hands",0,75,30,0,0,"blunt-hands");
  add_attack("feet", 0, 25, 40, 0, 0, "blunt-feet");
  add_ac("bing", "blunt", 15);
  add_ac("bing2", "sharp", 15);
  add_ac("bing3", "pierce", 15);
  add_property("player", 1);
  cols = 79;
  rows = 24;
  long_view = 1;
  last_log_on = time();
  race_ob = RACE_STD;
  sscanf(file_name(this_object()), "%s#", my_file_name);
} /* create() */


// Coded the 5th of may 1994 by Begosh@RotD
// Receiver for the chat


void receive_message( string str, string class )
{
   strlen( class ) ? receive( ( fix_string( sprintf("\n%s%-=*s\n",
      class, cols - strlen( class ), str ) ) ) ) :
      receive( fix_string( sprintf("%-=*s", cols,
          process_string( str ) ) ) );
}

string query_start_pos() {
  string start_pos;

  if (this_player()->query_creator()) {
    start_pos = "/w/"+query_name()+"/workroom";
    if (!find_object(start_pos))
      if (catch(call_other(start_pos,"??")))
        start_pos = "/w/common";
    if (!find_object(start_pos))
      if (catch(call_other(start_pos,"??")))
        start_pos = START_POS;
    return start_pos;
  }
  if(race_ob != "/std/races/drow") {
     if (!guild_ob)
     {
       start_pos = START_POS;
     }
     else
     {
      catch(start_pos = (string)guild_ob->query_start_pos());
     }
 }else{
    catch(start_pos= (string)race_ob->query_start_pos());
 }
  if (!find_object(start_pos))
    if (catch(call_other(start_pos,"??")))
      start_pos = START_POS;
  return start_pos;
} /* query_start_pos() */

string query_cap_name() { return capitalize(query_name()) ;}

void move_player_to_start(string bong, int new, string c_name) {
int tmp;
    mapping mail_stat;
object money;
string s;
  if (file_name(previous_object())[0..13] != "/secure/login#") {
    notify_fail("You dont have the security clearance to do that.\n");
    return ;
  }
/* some stupid test to make sure that the previous object is /secure/login. */
  seteuid("Root");
  set_name(bong);
  if (query_property("guest"))
    log_file("ENTER", sprintf("Enter : %15-s %s (guest)[%d] [%s]\n",
                name, ctime(time()), time(),
                (query_ip_name()?query_ip_name():query_ip_number())));
  else
    log_file("ENTER", sprintf("Enter : %15-s %s[%d]\n",
                name, ctime(time()), time()));
  restore_object("/players/"+name[0..0]+"/"+name,1);
  colour_map=0;
  // Added by Dyraen, to help track down multi-chars.
  if (query_property("new")) {
    log_file("NEW_PLAYER", sprintf("%15-s %s %s\n",name,ctime(time()),
             (query_ip_name()?query_ip_name():query_ip_number())));
    remove_property("new");
  }
  add_property("player", 1);
  cap_name = query_cap_name();
  set_short(cap_name);
  if (!cols) cols = 79;
  add_property("determinate", "");
  if (!sites) sites = ([ ]);
  if (sites[query_ip_number()]) {
    if (sites[query_ip_number()] == "not resolved" &&
        query_ip_name()) {
      sites[query_ip_number()] = query_ip_name();
    }
  } else {
    sites[query_ip_number()] = query_ip_name()?query_ip_name():
                                               "not resolved";
  }
  if (this_player()->query_creator())
    seteuid(name);
  else
    seteuid("PLAYER");
  write("You last logged in from "+last_on_from+".\n");
  last_on_from = query_ip_name(this_object())+" ("+
                 query_ip_number(this_object())+")";
  bonus_cache = ([ ]);
  level_cache = ([ ]);
  if (time_on < -500*24*60*60)
    time_on += time();
  if (time_on > 0)
    time_on = 0;
  time_on += time();
  guild_joined += time();
  save_counter =0;
  start_player();
/* ok this moves us to the other player... ie we are already playing. */
  write("Welcome to the all new Realms of the Dragon, "+cap_name+".\n");

  cat("doc/NEWS"); 
  if (!last_pos || catch(call_other(last_pos, "??"))) {
    last_pos = query_start_pos();
  }
  if (new) adjust_hp(500);
  move(last_pos);
  event(users(), "inform", query_cap_name() +
    " enters " + (query_property("guest")?"as a guest of ":"") + "the Realms of the Dragon"+
    (new?"%^RED%^ (New player)%^RESET%^":""),
    "logon");
  say(cap_name+" enters the game.\n", 0);
  show_view();
  no_time_left();
  last_pos->enter(this_object());

  money = clone_object(MONEY_OBJECT);
  money->set_money_array(money_array);
  money->move(this_object());

  if (query_property(PASSED_OUT_PROP))
  {
    life_passed_out = 1;
    call_out("remove_property", 10+random(30), PASSED_OUT_PROP);
  }

  init_after_save(); /* for effects.  Sorry about the name */
  curses_start();
    mail_stat = (mapping)POSTAL_D->mail_status(query_name());
    if(mail_stat["unread"]) {
    if(mail_stat["total"] == 1)
      write("\n        >>> Your only mail is unread! <<<\n");
    else write("\n        >>> "+mail_stat["unread"]+" of your "+
      mail_stat["total"]+" letters are unread. <<<\n");
    }

  if (query_property("dead"))
  {
    life_dead = 1;
    money = clone_object(DEATH_SHADOW);
    money->setup_shadow(this_object());
  }
  if(query_property("noregen"))
    DEATH_CHAR->person_died(query_name());
  exec_alias("login","");
  last_log_on = time();
  set_title((string)"obj/handlers/library"->query_title(query_name()));
  LOGIN_HANDLER->player_logon(bong);
  if (my_file_name != "/global/player")
    if (file_size("/w/"+name+"/"+PLAYER_ERROR_LOG) > 0)
      write("You have ERRORS in /w/"+name+"/"+PLAYER_ERROR_LOG+"\n");
} /* move_player_to_start() */

void start_player() {
  if (app_creator && my_file_name != "/global/player") {
    this_player()->all_commands();
    this_player()->app_commands();
  }
  if (creator && my_file_name != "/global/player") 
    this_player()->wiz_commands();
  reset_get();
  enable_commands();
  public_commands();
  parser_commands();
  handle_commands();
  force_commands();
  race_guild_commands();
  soul_commands();
  event_commands();
  finger_commands();
  communicate_commands();
  living_commands();
  spell_commands();
  logging_commands();
  editor_commands();
  set_living_name(name);
  set_no_check(1);
  set_con(Con);
  set_str(Str);
  set_int(Int);
  set_dex(Dex);
  set_wis(Wis);
  reset_all();
  current_path = home_dir;
  call_out("do_load_auto", 0);
  birthday_gifts(); /* check if birthday today and give gifts */
  set_heart_beat(1);

  if (query_wimpy() > 100)
  {
    set_wimpy(25);
  }
} /* start_player() */

void init() {
} /* init() */

int query_start_time() { return start_time; }

int do_load_auto() { 
  load_auto_load(auto_load, this_object());
} /* do_load_auto() */

void public_commands()
{
    add_action("restart_heart_beat", "restart", A_P_EMERGENCY);
  
  // operating commands -- cannot be externalized
  // available when incapacitated (dead, drunk, passed out)
    add_action("cmd_harass",    "harass",   A_P_ALWAYS);
    add_action("help_func",     "help",     A_P_ALWAYS);
    add_action("x_quit",        "quit",     A_P_ALWAYS);
    add_action("last",          "last",     A_P_ALWAYS);
    add_action("do_refresh",    "refresh",  A_P_ALWAYS);
    add_action("do_retire",     "retire",   A_P_ALWAYS);
    add_action("save",          "save",     A_P_ALWAYS);
/*  add_action("do_su",         "su",       A_P_ALWAYS); */

  // player actions -- many externalized
  // cannot be done while incapacitated
//  add_action("bury","bury");
    add_action("do_cap", "cap");
//  add_action("encumbrance_by_ish","encumbrance");
//  add_action("sheet", "sheet");
//  add_action("kill","kill");
//  add_action("stop","stop");
    add_action("invent","inventory"); //not worth making them external
    add_action("invent","i");
    add_action("rearrange", "rearrange");
    add_action("review", "review");
//  add_action("score","score");
//  add_action("setmin", "setmin");
//  add_action("setmout", "setmout");
//  add_action("setmmin", "setmmin");
//  add_action("setmmout", "setmmout");
//  add_action("toggle_wimpy", "wimpy");
    add_action("monitor", "mon*itor");
//  add_action("consider", "con*sider");
    add_action("do_teach","teach");
    add_action("do_learn", "learn" );

    add_action("dispel", "dispel" );    /* added by Lotus for spell code */

    add_action("brief_verbose", "brief");
    add_action("x_examine","ex*amine");
    add_action("x_glance", "g*lance");
    add_action("x_look","l*ook");
    add_action("brief_verbose", "verbose");

  // Asmodean: Added for the cmds system
    add_action("do_cmd", "*", A_P_CMD);
  
} /* public_commands() */

/* Added by Lotus to remove shadows added by spells */
/* This should be overridden by the shadow but is needed
   to avoid adding extra code and prevent bug possibility
   to all shadow spells */
int dispel() { 
    write("You don't have any spell on that you can dispel.\n");
    return 1; 
}

/* Harass log command added by Kelaronus - 1/31/95 */
void set_harass_block(int i)
{
  if(!i){
    return hblock;
  }
  if(!"/secure/master"->high_programmer(geteuid(this_player()))){
    return 0;
  }
  hblock = i;
  return hblock;
}

int query_harass_block()
{
  return hblock;
}

cmd_harass(string timestr)
{
  int mins;
  string name;
  if((int)this_object()->query_harass_block() != 0){
    notify_fail("Due to abuse of this command, your use of it has been "+
        "removed.\n");
    return 0;
  }
  name = this_player()->query_cap_name();
  if (!timestr || !sscanf(timestr, "%d", mins)) {
    notify_fail("Usage: harass <minutes>\n");
    return 0;
  }
  write("Harassment Log started at " + ctime(time()) +
        ".\nEverything you see for the next " + mins +
        " minutes will be logged.\n");
  add_property("harass", time() + 60 * mins);
  log_file("harass/"+name+".log", "Harassment Log started: "+
     ctime(time()) + "\n"); 
  return 1;
}


int invent() {
  write(query_living_contents(1));
  return 1;
}


int review() {
  write("Entry  : " + msgin + "\n");
  write("Exit   : " + msgout + "\n");
  write("MEntry : " + mmsgin + "\n");
  write("MExit  : " + mmsgout + "\n");
// Latter seems to be hosed -- Ink
//  write("Editor : " + editor + "\n");
  return 1;
}

string short(int dark) {
  string str;

  if (!interactive(this_object()))
    str = "The net dead statue of ";
  else
    str = "";
  return str+::short(dark);
}

int query_savable() {
  if (query_creator()) return 1;
  if (-(time_on-time()) >= MIN_AGE) return 1;
  if (guild_ob && 
      guild_ob->query_level(this_object()) >= MIN_LEVEL) return 1;
  if (!guild_ob && query_skill("fighting") >= MIN_LEVEL) return 1;
  return 0;
} /* query_savable() */

nomask int save() {
  if (my_file_name == "/global/player" || query_verb() == "save")
    tell_object(this_object(), "Saving...\n");
  save_me();
  return 1; 
}

void save_me() {
  object ob;
  mixed old;

  if (query_property("guest")) {
    write("But not saving for guests... sorry.\n");
    return ;
  }
  if ((ob = present("Some Money For Me", this_object())))
    money_array = (mixed *)ob->query_money_array();
  else
    money_array = ({ });
  if (guild_ob)
    guild_ob->player_save();
  if (race_ob)
    race_ob->player_save();
  old = geteuid();
  if (environment())
    last_pos = file_name(environment());
  else
    last_pos = query_start_pos();
  if (last_pos[0..2] == "/w/") {
    if (last_pos[3..strlen(name)+2] != name)
      if(!last_pos->query_valid_save(name))
        last_pos = query_start_pos();
  }
  auto_load = create_auto_load(all_inventory());
  time_on -= time();
  guild_joined -= time();
  save_counter = 0;
  seteuid("Root");
  catch(save_object("/players/"+name[0..0]+"/"+name,1));
  seteuid(old);
  time_on += time();
  guild_joined += time();
} /* save_me() */

int x_quit()
{
  object *ob, money;
  object frog, frog2;
  int i;

  if (this_player() != this_object()) {
    time_left = FOREVER_TIME; /* Make sure it gets done... */
    return command("quit");
  }
  last_log_on = time();
  log_file("ENTER", sprintf("Exit  : %15-s %s[%d]\n", name, 
       ctime(time())+ (query_property("guest")?"(guest)":""), time()));
  catch(editor_check_do_quit());
  write("Thanks for playing. See you next time..\n");
  say(cap_name + " left the game.\n");
  if (retired)
    event(users(), "inform", query_cap_name() + 
          " left the Realms of the Dragon (retired)", "logon");
  else
    event(users(), "inform", query_cap_name() +
          " left the Realms of the Dragon", "logon");
  LOGIN_HANDLER->player_logout(query_name());
  if (race_ob)
    catch(race_ob->player_quit(this_object()));
  if (guild_ob)
    catch(guild_ob->player_quit(this_object()));
  curses_quit();
  save_me();
/* get rid of the money....
 * we dont want them taking it twice now do we?
 */
  if ((money = present("Some Money For Me", this_object())))
    money->dest_me();
  frog = first_inventory(this_object());
  while (frog) {
    frog2 = next_inventory(frog);
    if (frog->query_auto_load() ||
    frog->query_static_auto_load())
      frog->dest_me();
    frog = frog2;
  }
  transfer_all_to(environment());
  ob = all_inventory(this_object());
  for (i=0;i<sizeof(ob);i++)
    ob[i]->dest_me();
  dest_me();
  return 1;
} /* quit() */


int quit()
{
  x_quit();
}


int do_cmd(string tail)
{
     string verb, t;

     sscanf(tail, "%s %s", verb, t);
     if(!verb)
         verb = tail;

    return (int)CMD_HANDLER->cmd(verb, t, this_object());
}/* do_cmd() */

 
string query_title() { return title; }
void set_title(string str) { title = str; }

string query_atitle() { return al_title; }
void set_atitle(string str) { al_title = str; }

void set_deaths(int death) { deaths = death;}
int query_deaths() { return deaths; }

void set_name(string str) {
  if (name && name != "object")
    return ;
  name = str;
  set_living_name(name);
  set_main_plural(name);
} /* set_name() */

string long(string str, int dark) {
  string s;

  if (str == "soul") {
    return (string)"/obj/handlers/soul"->query_long();
  }
  if (str == "sun" || str == "moon" || str == "weather") {
  }
/*      Because I like to see myself--what a narcissist!  */
  if (this_player() == this_object())
    write("Looking at yourself again? What a narcissist!\n");
  else
    if (!this_player()->query_invis() &&
        !this_player()->query_invis_spell() &&
        !this_player()->query_hide_shadow())
      tell_object(this_object(), this_player()->query_cap_name()+" looks at you.\n");
  s = "You see "+cap_name;
  if (guild_ob)
    s += " "+(string)guild_ob->query_title(this_object());
  if (al_title)
    s += " ("+al_title+"), \n";
  else
    s += ",\n";
  if (race_ob)
    s += (string)race_ob->query_desc(this_object());
  if (desc && desc != "")
    s += query_cap_name() + " " + desc+"\n";
  else if (race_ob)
    s += (string)race_ob->query_long();
  s += capitalize(query_pronoun())+" "+health_string()+".\n";
  labels = labels - ({ 0 });
  if (sizeof(labels))
    s += "There is "+query_multiple_short(labels)+" stuck on "+
     query_objective()+".\n";
  s += calc_extra_look();
  s += query_living_contents(0);
  return s;
} /* long() */

/* second life routine... handles the player dieing. */
int second_life() {
  string str;
  int i, no_dec;
  object tmp;

  make_corpse()->move(environment());
  str = query_cap_name() + " killed by ";
  if (!sizeof(attacker_list))
    str += this_player()->query_cap_name()+" (with a call)";
  else
    for (i=0;i<sizeof(attacker_list);i++)
      if (attacker_list[i]) {
    str += attacker_list[i]->query_name()+"<"+geteuid(attacker_list[i])+"> ";
    attacker_list[i]->stop_fight(this_object());
    no_dec += interactive(attacker_list[i]);
      }
  log_file("DEATH", ctime(time())+": "+str + "\n");
  event(users(), "inform", str, "death");
  attacker_list = ({ });
  call_out("effects_thru_death", 0); /* this could be longer with no hassle */
/*
  weapon = 0;
*/
  if (!no_dec)
    deaths++;
  if (deaths > max_deaths) {
    write("You have died too many times.  I am sorry, your name will be "+
      "inscribed in the hall of legends.\n");
    shout(this_object()->query_cap_name()+" has just died forever. "+
      "All mourn "+this_object()->query_possessive() +" passing.\n");
    LIBRARY->complete_death(query_name());
  }
/* oh dear complete death ;( */
  say(cap_name+" collapses in a lifeless heap.\n");
  save_me();
  DEATH_CHAR->person_died(query_name());
  set_hp(0);
  set_gp(0);
  set_xp(0);
  if (!no_dec) {
    contmp = -2;
    strtmp = -2;
    dextmp  = -2;
    inttmp = -2;
    wistmp = -2;
  }
  this_object()->add_property("dead",1);
    tmp = clone_object(DEATH_SHADOW);
  tmp->setup_shadow(this_object());
  return 1;
} 

void remove_ghost() {
  if (this_object()->query_cabbage_shadow()) {
    write("But they are a cabbage!!!!\n");
    return ;
  }
  if (deaths > max_deaths) {
    if (this_player() != this_object()) {
      tell_object(this_object(), this_player()->query_cap_name()+
          " tried to raise you, but you are completely dead.\n");
      tell_object(this_player(), query_cap_name()+
          " is completely dead, you cannot raise him.\n");
    } else
      tell_object(this_object(), "You are completely dead.  You cannot "+
          "be raised.\n");
    say(query_cap_name()+" struggles to appear in a solid form, but fails.\n");
    return ;
  }
  remove_property("dead");
  tell_object(this_object(), "You reappear in a more solid form.\n");
  say(query_cap_name() + " appears in more solid form.\n");
  this_object()->dest_death_shadow();
} /* remove_ghost() */


void net_dead() {
  if (name == "guest" || name == "root") {
    say(cap_name + " just vanished in a puff of logic.\n");
    x_quit();
  } else {
    say(cap_name+" goes white, looks very chalky and turns into a "+
                 "statue.\n");
    event(users(),"inform","%^GREEN%^"+query_cap_name()+" has lost "+
          query_possessive() + " %^GREEN%^link%^RESET%^", "link-death");
    save_me();
    for (i=0;i<sizeof(attacker_list);i++)
      attacker_list[i]->stop_fight(this_object());
  }
  if(time() - last_command > 0 ) { /* used to be MAX_IDLE */
    say(cap_name+" has been idle for too long, "+query_pronoun()+
                 " vanishes in a puff of boredom.\n");
    x_quit();
    return ;
  }
} /* net_dead() */

void idle_out() {
  if (((time() - last_command < MAX_IDLE) || this_object()->query_creator()
  || sizeof(users()) < 3) &&
     interactive(this_object())) {
//    last_command = time();
    return ;
  }
  tell_room(environment(this_object()),cap_name+
            " has been idle for too long, "+this_object()->query_pronoun()+
            " vanishes in a puff of boredom.\n", ({this_object()}));
  tell_object(this_object(),"You idled out sorry.  Come back again!\n");
  x_quit();
} /* idle_out() */

void run_away() {
  mixed *direcs;
  int i, bong;

  direcs = (mixed *)environment()->query_dest_dir();
  while (!bong && sizeof(direcs)) {
    i = random(sizeof(direcs)/2)*2;
    bong = command(direcs[i]);
    if (!bong)
      direcs = delete(direcs, i, 2);
    else
      write("You flee from the scene.\n");
  }
  if (!bong)
    write("You tried to run away, but no matter how much you scrambled,"
        + " you couldn't find any exits.\n");
}


/*
 * called every HB while player is alive
 */
void heart_beat() 
{
  int i, gp_bonus, hp_bonus;

  command_hb();  // process queued commands

  /* The new autosave:
   * Baldrick feb '94.
   */
  if (++save_counter >= TIME_BETWEEN_SAVES)
  {
    save_counter = 0;
    save();
    update_tmps();
  }

  if (time() - last_command > MAX_IDLE)
    if (!interactive(this_object()) || my_file_name != "/global/lord") {
      return ;
 // call_out("idle_out",120);
    }

  if ( life_dead )
  {
    // nothing happens while dead {Laggard}
    return;
  }

  gp_bonus = guild_ob ? guild_ob->query_gp_regen_bonus() : 0;
  hp_bonus = (int)query_con();

  health_heart_beat( gp_bonus, hp_bonus );

  /* Added by Akira to support adverse environments like drowning in water */
  adverse_environment(this_object());

  if ( life_passed_out )
  {
    // no actions while passed out {Laggard}
    return;
  }

  // note that the player could run away at the odd chance of 0 hp,
  // from the adverse environment (above)  {Laggard}
  if ( hp < wimpy_hp  &&  sizeof(attacker_list) )
  {
    run_away();
  }

  if ( drunken_heart_beat(intox_level)
    && time_left > 0)
  {
    if (sizeof(attacker_list))
    {
      attack();
      time_left -= (int)environment()->attack_speed();
    } else {
      attack();
      if (sizeof(attacker_list))
        time_left -= (int)environment()->attack_speed();
    }
  }
  do_spell_effects(attackee);

  if ( monitor  &&  sizeof(attacker_list)
    &&  (hb_hp_saved > hp  ||  hb_gp_saved > gp) )
  {
      hb_hp_saved = hp;
      hb_gp_saved = gp;
      // Added xp -- Ink
      write("Hp: "+ hp +"  Gp: " + gp + "  Xp: "+query_xp()+"\n");
  }

  if ( ++hb_sp > 3 )
  {
    hb_sp = 0;
    hb_hp_saved = max_hp;
    hb_gp_saved = max_gp;

    if ( social_points < max_social_points)
    {
      social_points++;
    }

    if ( ++hb_sp_max > 127 )  // 512 = (128 * 4)
    {
      hb_sp_max = 0;
      max_social_points++;
    }
  }
} /* heart_beat() */


int monitor(string str) {
  if (!str)
    monitor = !monitor;
  else if (str == "on")
    monitor = 1;
  else if (str == "off")
    monitor = 0;
  else {
    notify_fail("Syntax: monitor <on/off>\n");
    return 0;
  }
  if (monitor)
    write("Your hit point monitor is on.\n");
  else
    write("Your hit point monitor is off.\n");
  return 1;
}

int query_invis()
{
  if(this_player() == this_object())
    return invis;

  if(invis == -1 && !this_player()->query_creator())
    return 2;
  return invis;
}

string query_invis_string()
{
  switch(invis)
  {
  case 0:
    return "Visible";
  case 1:
    return "Normal Invisibility";
  case -1:
    return "Invis level 1";
  case 2:
    return "Invis level 2";
  default:
    return "Invalid Invis level";
  }
}

int help_func(string str) {
  string rest;
  mixed i;

  if (!str) return do_help(0);
  if (sscanf(str, "spell %s", rest) == 1) {
    i = help_spell(rest);
    if (i) {
      write(i);
      return 1;
    }
    notify_fail("No such spell as '"+rest+"'\n");
    return 0;
  }
  if (sscanf(str, "command %s", rest) == 1) {
    i = help_command(rest);
    if (i) {
      write(i);
      return 1;
    }
    notify_fail("No such command as '"+rest+"'\n");
    return 0;
  }
  i = do_help(str); /* calling /global/help.c */
  if (!i)
    if ((i = help_spell(str)))
      write(i);
  if (!i)
    if ((i = help_command(str)))
      write(i);
  return i;
}


int query_wizard() { return creator; } /* need this fo rthe gamed driver */
int query_app_creator() { return app_creator; }


void set_al(int i) { alignment = i; }
int query_al() { return alignment; }


int adjust_align(int i) {
  if (alignment != 0)
    alignment += (i*10)/(alignment<0?-alignment:alignment);
  else
     alignment += i;
  /* can't have this call when I have rmoved the function.. Baldrick.*/
  /*
  reset_align_title();
  */
  return alignment;
} /* adjust_align() */

void set_time_on(int num) { time_on=num;}
int query_time_on() { return time_on - time(); }
int query_last_joined_guild() { return guild_joined - time(); }

int check_dark(int light) {
  int i;

  if (race_ob)
    if (catch(i = (int)race_ob->query_dark(light)))
      race_ob = RACE_STD;
    else
      return i;
  return (int)race_ob->query_dark(light);
}

int query_level() {
  if (guild_ob)
    return (int)guild_ob->query_level(this_object());
  return query_skill("fighting");
  return 0;
}

int restart_heart_beat()
{
  set_heart_beat(1);
  reset_queue("");
  write("Ok, heart_beat restarted.\n");
  return 1;
}

void set_snoopee(object ob) { snoopee = ob; }
object query_snoopee() { return snoopee; }


void set_creator(int i) {
  if (file_name(previous_object()) != "/secure/master") {
    write("Illegal attempt to set creator!\n");
    log_file("ILLEGAL", this_player(1)+" Illegal attempt to set_creator "+
            "at "+ctime(time())+" from "+file_name(previous_object())+"\n");
  }
  creator = i;
  app_creator = i;
if(i == 1){
  home_dir = "/w/"+name;
}else{
home_dir="";
}
  save_me();
}

int query_prevent_shadow(object ob) {
  if (function_exists("query_prevent_shadow", ob) ||
      function_exists("query_name", ob) ||
      function_exists("query_creator", ob) ||
      function_exists("query_app_creator", ob) ||
      function_exists("query_hidden", ob) ||
      function_exists("dest_me", ob) ||
      function_exists("save_me",ob))
    return 1;
  return 0;
}

int query_max_deaths() { return max_deaths; }
void set_max_deaths(int i) { max_deaths = i; }
int adjust_max_deaths(int i) { return (max_deaths += i); }

/* Includeding new hack for parse_command ;) */
move(object dest, string msgin, string msgout) {
  int i;
  object env;

  if ((env = environment()))
    WEATHER->unnotify_me(environment());
  i = ::move(dest, msgin, msgout);
  if (environment())
    WEATHER->notify_me(environment());
  if (!i)
    me_moveing(env);
  return i;
}


static int do_refresh(string str) {
  if (!str || str != "me") {
    notify_fail("Please read the docs before using this command.\n");
    return 0;
  }
  write("WARNING!  Make sure you have read the docs before doing this!\n\n"+
        "Are you sure you wish to refresh yourself? ");
  input_to("refresh2");
  return 1;
}

static int refresh2(string str) {
  str = lower_case(str);
  if (str[0] != 'n' && str[0] != 'y') {
    write("Pardon?  I do not understand.  Do you want to refresh yourself? ");
    input_to("refresh2");
    return 1;
  }
  if (str[0] == 'n') {
    write("Ok, not refreshing.\n");
    return 1;
  }
  write("Doing refresh.\n");
  channels = ([]);
  if(guild_ob) guild_ob->leave_guild();
  Str = 13;
  Con = 13;
  Int = 13;
  Wis = 13;
  Dex = 13;
  intbon=dexbon=wisbon=strbon=conbon =0;
  race_ob->start_player();
  inttmp = dextmp = wistmp = strtmp = contmp = 0;
 
  skills = ({ });
  spells = ({ });
  set_guild_ob(0);
  reset_health();
  set_xp(0,this_object(),"refresh");
  alignment = 0;
//alignment reset added by Rahvin 9-6-95
  deaths = 0;
  max_deaths = 7;
  level_cache = ([ ]);
  bonus_cache = ([ ]);
  gr_commands = ([ ]);
  known_commands = ({ "skills" });
   save();
  race_guild_commands();
  stat_cache = ([ ]);
  remove_property("stats_rearranged");
  this_player()->remove_ghost();
  write("Done.\n");
 if (query_property("guest"))
   map_prop = (["guest":1,"player":1,"determinate":"",]);
 else
   map_prop = (["player":1,"determinate":"", ]);
  reset_all();
  save();
  say(cap_name+" refreshes "+query_objective()+"self.\n");
  return 1;
}

/*** function to remove known_commands to just skills ***/
/** Eerevann, because I couldn't find a function that could do this ***/

int reset_known_commands()
{
  known_commands = ({"skills"});
    write("Feesh\n");
    return 1;
}


/* for creators who are playing as players */
int query_creator_playing() { return creator; }
int query_player() { return 1; }


/* Installed by Begosh 23-mar-95 */
int do_teach( string str )
{
    return "/std/commands/teach.c"->teach( str );
}

int do_learn( string str )
{
    return "/std/commands/learn.c"->learn( str );
}


string query_object_type() {
  if (creator)
    return "A";
  return " ";
} /* query_object_type() */


int do_cap(string str) {
  if (!str) {
    notify_fail("Syntax: "+query_verb()+" <cap_name>\n");
    return 0;
  }
  if (lower_case(str) != name) {
    notify_fail("You must have the same letters in your capitalized name.\n");
    return 0;
  }
  cap_name = str;
  write("Set capitalised name to "+cap_name+".\n");
  return 1;
} /* do_cap() */


void setenv(string cle,string val) {
    if(!env_var) env_var = ([]);
    env_var[cle] = val;
}

string getenv(string cle) { return (env_var ? env_var[cle] : 0); }

int do_retire() {
  /* the real checks is in the command */
  return "/secure/master"->try_retire();
} /* void do_retire */


// Added trimmed cmd versions too, they are either useful or used by other
// objects.

int sheet()
{
  return do_cmd ("sheet");
}


int look_me(string arg)
{
  log_file( "look_me", ctime(time())
                + " " + (previous_object() ? file_name(previous_object()) : "*0*")
                +"\n");
  return x_look(arg);
}


// following added to be used with the who command
// to see when someone is mailing or posting. Dyraen - June '95
int set_mailing(int i) { mailing = i; }
int set_posting(int i) { posting = i; }
int set_retired(int i) { retired = i; }

int query_mailing() { return mailing; }
int query_posting() { return posting; }
int query_retired() { return retired; }

mapping query_sites() {
  if ("/secure/master"->query_lord(geteuid(this_player(1))))
    return sites;
  return 0;
} /* query_sites() */

mixed *query_commands() {
string euid;
 
  euid = geteuid(previous_object());
  if (this_object() != previous_object()) {
    if ("/secure/master"->high_programmer(euid))
      return commands();
    if ("/secure/master"->query_lord(euid) &&
        !"/secure/master"->high_programmer(geteuid(this_object())))
      return commands();
    if (previous_object()->query_creator() &&
        !"/secure/master"->query_lord(geteuid(this_object())))
      return commands();
    return ({ });
  }
  return commands();
} /* query_commands() */