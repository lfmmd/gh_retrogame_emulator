
cpu_init()
{
  //jz_dev = open("/dev/mem", O_RDWR);
  //jz_cpmregl=(unsigned long  *)mmap(0, 0x80, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x10000000);
  //jz_emcregl=(unsigned long  *)mmap(0, 0x90, PROT_READ|PROT_WRITE, MAP_SHARED, jz_dev, 0x13010000);
  //jz_emcregs=(unsigned short *)jz_emcregl;

}

void 
cpu_deinit()
{
  //cpu_set_clock( GP2X_DEF_CLOCK );
  //munmap((void *)jz_cpmregl, 0x80); 
  //munmap((void *)jz_emcregl, 0x90);   
  //close(jz_dev);
  //fcloseall();
  //sync();
}

static void
loc_set_clock(int clock_in_mhz )
{
  //if (clock_in_mhz >= GP2X_MIN_CLOCK && clock_in_mhz <= GP2X_MAX_CLOCK) {
    //pll_init( clock_in_mhz * 1000000 );
  //}
}

//static unsigned int loc_clock_in_mhz = GP2X_DEF_CLOCK;

void 
cpu_set_clock(unsigned int clock_in_mhz)
{
  //if (clock_in_mhz == loc_clock_in_mhz) return;
  //loc_clock_in_mhz = clock_in_mhz;
  
  //loc_set_clock(clock_in_mhz);

  //return;
}

unsigned int
cpu_get_clock()
{
  //return loc_clock_in_mhz;
}

