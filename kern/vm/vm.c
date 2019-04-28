#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>




/* Place your page table functions here */
/*
paddr_t pt_transation(vaddr_t vaddress) {
	int vpn1 = vaddress >> (10 + 12);//Shift the last 10 bit to be the only bits
	int vpn2 = (vaddress << 10) >> (10 + 12); //Shift the 10 middle bit to be the only bits
	off_t offset = (vaddress << (10 + 10)) >> (10 + 10);//Shift the first bit to be the only bits
	paddr_t padress = pagetable[vpn1][vpn2]->paddr_t[offset];
}
*/
void vm_bootstrap(void)
{
    /* Initialise VM sub-system.  You probably want to initialise your 
       frame table here as well.
    */
	/*
	lvl2PT_lock = lock_create("page_table_lock");
	//No memory left to create lock
	if (lvl2PT_lock == NULL) {
		return;
	}
	lvl2PT = kmalloc(sizeof(level2PageTable));
	//No memory left to crete the page table
	if (lvl2PT == NULL) {
		return;
	}
	struct frame_table ft;
	ft.size = ;//something
	ft.start = ;//something
	*/
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void) faulttype;
    (void) faultaddress;

    panic("vm_fault hasn't been written yet\n");

    return EFAULT;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

