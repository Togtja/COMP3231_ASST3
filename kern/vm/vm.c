#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <current.h> //for curproc
#include <spl.h> //for spl stuff;
#include <proc.h> //to get adressspace from proc
#include <synch.h> //For the locks





/* Place your page table functions here */

int mpt_create(addrspace as, vaddr_t vaddress) {
	int vpn1 = vaddress >> (10 + 12);//Shift the last 10 bit to be the only bits
	int vpn2 = (vaddress << 10) >> (10 + 12); //Shift the 10 middle bit to be the only bits
	//off_t offset = (vaddress << (10 + 10)) >> (10 + 10);//Shift the first bit to be the only bits
	
	vaddr_t frame_nr = alloc_kpages(1);
	if (frame_nr == 0) {
		//Ran out of pages
		return EFAULT;
	}
	
	if (mpt[vpn1] == NULL) {
		mpt[vpn1] = kmalloc(sizeof(struct root_level));
	}
	if (mpt[vpn1]->mlp[vpn2] == NULL) {
		mpt[vpn1]->mlp[vpn2] = kmalloc(sizeof(struct multi_level_pagetable));
	}

	mpt[vpn1]->mlp[vpn2]->mpt_asid = as->asid;
	mpt[vpn1]->mlp[vpn2]->vpn = vaddress;
	mpt[vpn1]->mlp[vpn2]->ppn = KVADDR_TO_PADDR(frame_nr & PAGE_FRAME);// | offset;
	return 0;
}

paddr_t findInMPT(addrspace as, vaddr_t vaddress) {
	(void)as;
	int vpn1 = vaddress >> (10 + 12);//Shift the last 10 bit to be the only bits
	if (vpn1 >= 1024) return 0;
	if (mpt[vpn1] == NULL) return 0;
	int vpn2 = (vaddress << 10) >> (10 + 12); //Shift the 10 middle bit to be the only bits
	if (vpn2 >= 1024) return 0;
	if (mpt[vpn1]->mlp[vpn2] == NULL) return 0;
	//if (mpt[vpn1]->mlp[vpn2]->mpt_asid == as->asid) {
		return mpt[vpn1]->mlp[vpn2]->ppn;
	//}
	//return 0;
	
}

bool region_valid(struct as_page* as_page, vaddr_t vdr) {
	if (vdr >= USERSTACK) {
		return false;
	}
	struct as_page* curr = as_page;
	int i = 0;
	while (curr != NULL) {
		if ((curr->vaddress & PAGE_FRAME) <= vdr && ((curr->vaddress >> 12) + curr->size) << 12 > vdr) {
			return true;
		}
		i++;
		curr = curr->next;
	}
	return false;
}


void vm_bootstrap(void)
{
    /* Initialise VM sub-system.  You probably want to initialise your 
       frame table here as well.
    */
	as_lock = lock_create("as_lock");

}

int
vm_fault(int faulttype, vaddr_t faultaddress) {
	
	//If faulttype is readonly efault
	faultaddress &= PAGE_FRAME;
	switch (faulttype) {
	case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		return EFAULT;
	case VM_FAULT_READ:
	case VM_FAULT_WRITE:
		break;
	default:
		return EINVAL;
	}
	
	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}
	struct addrspace *as;
	as = proc_getas();

	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}
	//Look up in page table
	int spl;
	//lock_acquire(as_lock);
	paddr_t ppn = findInMPT(as, faultaddress);
	//If valid load TBL
	if (ppn != 0) {
		spl = splhigh();
		tlb_random(faultaddress, ppn | TLBLO_DIRTY | TLBLO_VALID);
		splx(spl);
		//lock_release(as_lock);
		return 0;
	}
	//Else Look up region (as_page)
	else {
		//if valid region allocate frame, zero fill, insert PTE and load TLB
		if (region_valid(as->start, faultaddress)) {
			//Create a mpt for the adress 
			int result = mpt_create(as, faultaddress);
			if (result) {
				//Filed to create
				//lock_release(as_lock);
				return result;
			}
			ppn = findInMPT(as, faultaddress);
			spl = splhigh();
			tlb_random(faultaddress, ppn | TLBLO_DIRTY | TLBLO_VALID);
			splx(spl);
			//lock_release(as_lock);
			return 0;
		}
	}
	//else efault
	//lock_release(as_lock);
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

