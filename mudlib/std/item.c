/* Item.c 
 * Made by Baldrick.
 * to be used instead of /std/object on *EVERY* object that can be carried.
 * 
 * Right now it's only armour.c and weapon.c that inherits it.
 * and for now that's the only one needing it..
 * unless you have any wear / wieldable items.. the should also inherit this
 */

inherit "/std/basic/item_misc.c";
inherit "/std/chat";
inherit "/std/basic/condition";

status wearable = 1;
status holdable;
int size, armour_type;
int hands;  /* Hamlet */
int hbcount;



void set_holdable(int i) 
{ 
    holdable = i; 
    if (wearable && holdable)
	wearable = 0;
}

void set_wearable(int i) 
{ 
    wearable = i; 
    if (wearable && holdable)
	holdable = 0;
}
void set_armour_type(int i)
{
    armour_type = i;
}

int query_holdable() { return holdable; }
int query_wearable() { return wearable; }
int query_in_use() { return in_use; }
int query_armour_type() { return armour_type; }

void create()
{
    hbcount = 0;
    holdable = wearable = 0;
    size = 0;                 // Fix by Wonderflug
    ::create();
    hands = 1;
}

/* Sizes.. This will be used in wield and wear and later also by containers.
 * Will have it here and not in /std/object because the roomsize is something 
 * else.. 
 * Baldrick aug '95.
 */

void set_size(int i)
{
    size = i;
} /* void size */

int adjust_size(int i)
{
    size += i;
    return size;
} /* adjust_size */

int query_size()
{
    return size;
} 

/* Hamlet Sep 1995 */
void set_hands_needed(int i) 
{
    hands = i;
}

int query_hands_needed() { return hands; }


// Radix, needed for long()
/* Bah.
string cond_string() { return(0); }

*/
// Radix moved long() from /obj/weapon.c & armour.c to here.
// Also added calc_extra_look() cause it was unused (though inherited)
// Jan 18, 1996
string long(string s, int dark)
{
    string cond = cond_string();
    string xtra = calc_extra_look();
    if(!cond) cond = "";
    if(!xtra) xtra = "";
    return ::long()+xtra+cond;
}

/* Added by Bishop to handle turning on/off heartbeat for chatting items */

int check_heart_beat()
{
  object current, *here;
  string *parents;
  int FOUND, i;
  
/* Finds the room that it is in */
  current = environment(this_object());
  while (!FOUND)
    {
      parents = deep_inherit_list(current);
      for (i=0; i<sizeof(parents); i++)
        {
          if ( (parents[i]=="std/room.c") ||
               (parents[i]=="/std/room.c") )
            {
              FOUND = 1;
            }
        }
      if (!FOUND)
        {
          current = environment(current);
          if (!current)
            return 0;
        }
    } 
/* Are there any players here? */
  here = all_inventory(current);
  for (i=0;i<sizeof(here);i++)
    {
      if (interactive(here[i]))
/* If so, return 1 */    
      { return 1; }
    }
  return 0;
}
void heart_beat()
{
  hbcount++;
  /* Checks every 100 hbs to see if we can shut off */
  if (!(hbcount % 100))
    {
      hbcount=0;
      if (!check_heart_beat())
	{
	  set_heart_beat(0);
	}
    }
  /* Calls the chat routine in /std/chat.c */
  chat_now();
}

void init()
{
/* Are there players in the room?
 * If so, can I chat? 
 * If so, do I have a heartbeat?
 * If not, give me one */
  if (check_heart_beat())
  {
    if (query_chat_chance())
    { 
      if (!query_heart_beat(this_object()))
      { set_heart_beat(1); }
    }
  }
/* No players? Turn me off */
  else
  {
    if (query_heart_beat(this_object()))
    { set_heart_beat(0); }
  } 
  ::init();
}
