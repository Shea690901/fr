inherit "/std/room";

void setup()
{
        set_short("Troll Pit");

        set_long("This is a large pit where you see many bits which "+
                "the previous occupants perthaps thought of with some value. "+
                "From the bits of _total_ rubbish you wonder if you would "+
                "find something of use if you dug deeper.\n");
                                                       
        set_light(80); 

        add_item("Pit", "This is a dirty great stinking pit for christ's sake.\n");

        add_item("Rubbish", "Hmmm...Looks like good old Rubbish.\n");

        add_alias("pit", "Pit");
        add_alias("rubbish", "Rubbish");

        add_exit("South", "/w/leafstar/troll1.c", "door");
}