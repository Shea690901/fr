// Jan 4, 1997 - Iczer@Realms of the Dragon - Created
//
// Usage : peek <start line> [<# of lines>] filename
// If the <# of lines is ommitted then query_rows() - 1 will be
// used.
//

#include <standard.h>
#include <cmd.h>
inherit CMD_BASE;

void setup()
{
    position = 1;
}

string query_usage() { return "peek <start> [<# of lines>] file\n"; }

string query_short_help() {
  return "peek lets you look at any particular location in a file."
         "The first argument is the location where you want to "
         "start reading the file from.  The second argument is "
         "optional, it specifies how many lines are to be read.  "
         "If it is not specified, then the number of rows set for "
         "your player minus 1 will be used instead.  The third "
         "argument is the file that is to be looked at.\n";
}


protected int cmd(string str, object me)
{
    int start, offset, len;
    object *things;

    if (!str)
    {
      return notify_fail("Usage: peek <start> [<# of lines>] file\n");
    }

    // Parse the file name / object name
    if (sscanf(str, "%d %d %s", start, offset, str) != 3)
    {
        if (sscanf(str, "%d %s", start, str) == 2)
        {
            offset = me->query_rows() - 1;
        }
        else
        {
            return notify_fail("Usage: peek <start> <# of lines> file\n");
        }
    }

    // Peek at the file of an object present
    if (sizeof(things = me->wiz_present(str, this_player())))
    {
        int egg;

        str = file_name(things[0]);
        sscanf(str, "%s#%d", str, egg);
        if (file_size(str) <= 0)
            str += ".c";
    }

    // the file is not valid so return
    if (!str)
    {
        return notify_fail("tail <# of lines> file\n");
    }

    if (str[0..0] != "/")
    {
        str = me->query_current_path() + "/" + str;
    }

    seteuid(geteuid(this_player()));

    len = file_length( str );

    if (len <= 0)
    {
        return notify_fail("The file does not exist.\n");
    }

    if (len == 0)
    {
        return notify_fail("The file is empty.\n");
    }

    if (start < 0) start = 0;

    if ((start + offset) > len)
    {
        offset = len - start + 1;
    }

    if (len == 0)
    {
        return notify_fail("The file is empty.\n");
    }

    if (start < 0) start = 0;

    if ((start + offset) > len)
    {
        offset = len - start + 1;
    }


    write(""+str+" lines "+start+" to "+(start+offset)+"\n");

    str = read_file(str, start, offset);
    if (!str)
    {
        return notify_fail("Either start line or the offset into the file "
                           "was not valid.\n");
    }
    write(str);
    seteuid( 0 );

    return 1;
}
