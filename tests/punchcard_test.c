#include "../lib/punchcard.h"
#include <stddef.h>

static volatile uint16 *const state=(volatile uint16 *)0x0500;
static PunchcardBinding writer,reader,duplicate;
static void event(uint8 device,uint8 events)
{
    if (is_intr_enabled()) state[1]|=1;
    if (events&PUNCHCARD_EVENT_INSERTED) state[2]++;
    if (events&PUNCHCARD_EVENT_ROW) state[3]++;
    if (events&PUNCHCARD_EVENT_ERROR) state[1]|=2;
    (void)device;
}
static void tick(void) { __asm__ volatile(".byte 0x0f,0x01"); }
static void result(uint8 device,uint8 *row) { while (!punchcard_try_result(device,row)) tick(); }
void rom_main(void)
{
    disable_intr(); for (uint8 i=0;i<16;i++) state[i]=0;
    irq_init();
    if (!punchcard_register(&writer,PUNCHCARD_WRITER(0),event)
        || !punchcard_register(&reader,PUNCHCARD_READER(0),event)
        || punchcard_register(&duplicate,PUNCHCARD_WRITER(0),event)) state[1]|=4;
    enable_intr(); state[0]=1;
    while (!(punchcard_status(PUNCHCARD_WRITER(0))&PUNCHCARD_PRESENT)) tick();
    if (!punchcard_write_name(PUNCHCARD_WRITER(0),"Punchcard C test")) state[1]|=8;
    result(PUNCHCARD_WRITER(0),NULL);
    if (!punchcard_write_description(PUNCHCARD_WRITER(0),"Independent description")) state[1]|=4096;
    result(PUNCHCARD_WRITER(0),NULL);
    uint8 row[32];
    for (uint8 r=0;r<9;r++) {
        if (r==8) { if (!punchcard_next(PUNCHCARD_WRITER(0))) state[1]|=16; result(PUNCHCARD_WRITER(0),NULL); }
        for (uint8 i=0;i<32;i++) row[i]=r*32+i;
        if (!punchcard_write_row(PUNCHCARD_WRITER(0),row)) state[1]|=32;
        result(PUNCHCARD_WRITER(0),NULL);
    }
    if (punchcard_cards(PUNCHCARD_WRITER(0))!=2 || punchcard_size(PUNCHCARD_WRITER(0))!=512UL) state[1]|=64;
    if (!punchcard_eject(PUNCHCARD_WRITER(0))) state[1]|=128;
    result(PUNCHCARD_WRITER(0),NULL); state[0]=2;
    while (!(punchcard_status(PUNCHCARD_READER(0))&PUNCHCARD_PRESENT)) tick();
    punchcard_read_name(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),row);
    if (row[0]!='P' || row[9]!=' ') state[1]|=256;
    punchcard_read_description(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),row);
    if (row[0]!='I' || row[12]!='d') state[1]|=8192;
    punchcard_read_name(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),row);
    if (row[0]!='P') state[1]|=16384;
    for (uint8 r=0;r<16;r++) {
        if (r==8) { punchcard_next(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),NULL); }
        punchcard_read_row(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),row);
        for (uint8 i=0;i<32;i++) if (row[i]!=(r<9 ? (uint8)(r*32+i) : 0)) state[1]|=512;
    }
    if (punchcard_remaining(PUNCHCARD_READER(0))!=0 || !(punchcard_status(PUNCHCARD_READER(0))&PUNCHCARD_END)) state[1]|=1024;
    punchcard_eject(PUNCHCARD_READER(0)); result(PUNCHCARD_READER(0),NULL);
    disable_intr(); if (!punchcard_unregister(&writer) || !punchcard_unregister(&reader)) state[1]|=2048;
    state[0]=3; for (;;) hlt();
}
