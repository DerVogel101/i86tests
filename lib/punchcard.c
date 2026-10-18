#include "punchcard.h"
#include <stddef.h>

static PunchcardBinding *bindings;
static bool valid(uint8 device) { return device < 32; }
uint8 punchcard_status(uint8 d) { return valid(d) ? in8(IO_PUNCHCARD_STATUS(d)) : 0; }
uint8 punchcard_events(uint8 d) { return valid(d) ? in8(IO_PUNCHCARD_EVENTS(d)) : 0; }
uint8 punchcard_error(uint8 d) { return valid(d) ? in8(IO_PUNCHCARD_ERROR(d)) : 0; }
uint16 punchcard_cards(uint8 d) { return valid(d) ? in16(IO_PUNCHCARD_CARDS(d)) : 0; }
uint16 punchcard_remaining(uint8 d) { return valid(d) ? in16(IO_PUNCHCARD_REMAINING(d)) : 0; }
uint16 punchcard_current_card(uint8 d) { return valid(d) ? in16(IO_PUNCHCARD_CARD(d)) : 0; }
uint8 punchcard_current_row(uint8 d) { return valid(d) ? in8(IO_PUNCHCARD_ROW(d)) : 0; }
uint32 punchcard_size(uint8 d) { return (uint32)punchcard_cards(d) * 256UL; }
void punchcard_clear_events(uint8 d,uint8 events) { if (valid(d)) out8(IO_PUNCHCARD_EVENTS(d),events); }
void punchcard_clear_error(uint8 d) { if (valid(d)) out8(IO_PUNCHCARD_COMMAND(d),PUNCHCARD_CLEAR_ERROR); }

static bool request(uint8 d,uint8 command,const uint8 *row,uint8 metadata)
{
    if (!valid(d) || !command || (command & (command-1)) || command >= PUNCHCARD_ACK
        || ((command == PUNCHCARD_WRITE || command == PUNCHCARD_WRITE_NAME) && !row)) return false;
    bool enabled = is_intr_enabled(); disable_intr();
    uint8 status = punchcard_status(d);
    bool accepted = (status & (PUNCHCARD_PRESENT|PUNCHCARD_BUSY|PUNCHCARD_READY)) == PUNCHCARD_PRESENT;
    if (accepted) {
        out8(IO_PUNCHCARD_METADATA(d),metadata);
        if (row) for (uint8 i=0;i<32;i++) out8(IO_PUNCHCARD_DATA(d)+i,row[i]);
        out8(IO_PUNCHCARD_COMMAND(d),command);
    }
    if (enabled) enable_intr();
    return accepted;
}
bool punchcard_request(uint8 d,uint8 command,const uint8 *row) { return request(d,command,row,PUNCHCARD_METADATA_NAME); }
bool punchcard_read_row(uint8 d) { return punchcard_request(d,PUNCHCARD_READ,NULL); }
bool punchcard_write_row(uint8 d,const uint8 row[32]) { return punchcard_request(d,PUNCHCARD_WRITE,row); }
bool punchcard_read_name(uint8 d) { return punchcard_request(d,PUNCHCARD_READ_NAME,NULL); }
static bool write_text(uint8 d,const char *name,uint8 metadata)
{
    if (!name) return false;
    uint8 row[32]; for (uint8 n=0;n<32;n++) row[n]=0; uint8 i=0;
    while (i<32 && name[i]) { row[i]=(uint8)name[i]; i++; }
    if (i==32 && name[32]) return false;
    return request(d,PUNCHCARD_WRITE_NAME,row,metadata);
}
bool punchcard_write_name(uint8 d,const char *name) { return write_text(d,name,PUNCHCARD_METADATA_NAME); }
bool punchcard_read_description(uint8 d) { return request(d,PUNCHCARD_READ_NAME,NULL,PUNCHCARD_METADATA_DESCRIPTION); }
bool punchcard_write_description(uint8 d,const char *description) { return write_text(d,description,PUNCHCARD_METADATA_DESCRIPTION); }
bool punchcard_next(uint8 d) { return punchcard_request(d,PUNCHCARD_NEXT,NULL); }
bool punchcard_eject(uint8 d) { return punchcard_request(d,PUNCHCARD_EJECT,NULL); }
bool punchcard_try_result(uint8 d,uint8 *row)
{
    if (!valid(d)) return false;
    bool enabled = is_intr_enabled(); disable_intr();
    bool ready = (punchcard_status(d)&PUNCHCARD_READY) != 0;
    if (ready) {
        if (row) for (uint8 i=0;i<32;i++) row[i]=in8(IO_PUNCHCARD_DATA(d)+i);
        out8(IO_PUNCHCARD_COMMAND(d),PUNCHCARD_ACK);
    }
    if (enabled) enable_intr(); return ready;
}
static void received(IrqDeviceType type,uint8 slot)
{
    uint8 d = slot + (type == IRQ_DEVICE_PUNCHCARD_WRITER ? 16 : 0);
    for (PunchcardBinding *binding=bindings;binding;binding=binding->next) if (binding->device==d) {
        uint8 events = punchcard_events(d); punchcard_clear_events(d,events);
        if (events) binding->callback(d,events); return;
    }
}
bool punchcard_register(PunchcardBinding *binding,uint8 d,PunchcardCallback callback)
{
    if (!binding || !callback || !valid(d)) return false;
    bool enabled=is_intr_enabled(); disable_intr(); bool available=true;
    for (PunchcardBinding *p=bindings;p;p=p->next) if (p==binding || p->device==d) available=false;
    bool ok=available && irq_register_id(&binding->irq,d<16 ? IRQ_DEVICE_PUNCHCARD_READER : IRQ_DEVICE_PUNCHCARD_WRITER,d&15,received);
    if (ok) { binding->device=d; binding->callback=callback; binding->next=bindings; bindings=binding; }
    if (enabled) enable_intr(); return ok;
}
bool punchcard_unregister(PunchcardBinding *binding)
{
    if (!binding) return false;
    bool enabled=is_intr_enabled(); disable_intr(); bool removed=false;
    for (PunchcardBinding **p=&bindings;*p;p=&(*p)->next) if (*p==binding && irq_unregister(&binding->irq)) {
        *p=binding->next; binding->next=NULL; binding->callback=NULL; removed=true; break;
    }
    if (enabled) enable_intr(); return removed;
}
