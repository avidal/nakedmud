while (1) {
    print "Autorun started", `date`;
    open (SERVER, "NakedMud");
    while(<SERVER>) {
	
    }
    print "Game exited", `date`;
    sleep 5;
}
