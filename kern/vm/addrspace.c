/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <elf.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

//struct paddr_t** pagetable; //pagetable

struct addrspace * as_create(void) {
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}
	/*
	 * Initialize as needed.
	 */
	as->start = NULL;
	return as;
}

int as_copy(struct addrspace *old, struct addrspace **ret) {
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
	if (old->start == NULL) {
		//You are trying to copy a newly created address space
		//So just return the newly created newas as they are the same
		*ret = newas;
		return 0;
	}
	struct as_page * curr = old->start;
	struct as_page * newcurr = kmalloc(sizeof(struct as_page));
	if (newcurr->next == NULL) {
		return ENOMEM;
	}
	newcurr->vaddress = curr->vaddress;
	newcurr->size = curr->size;
	newcurr->mode = curr->mode;
	newcurr->pre_load_mode = curr->pre_load_mode;
	newcurr->next = NULL;
	newas->start = newcurr;
	
	while (curr->next != NULL) {
		newcurr->next = kmalloc(sizeof(struct as_page));
		if (newcurr->next == NULL) {
			return ENOMEM;
		}
		newcurr->next->vaddress = curr->next->vaddress;
		newcurr->next->size = curr->next->size;
		newcurr->next->mode = curr->next->mode;
		newcurr->next->pre_load_mode = curr->next->pre_load_mode;
		newcurr->next->next = NULL;
		curr = curr->next;
		newcurr = newcurr->next;
	}
	//DEBUG PRINTING
	
	curr = old->start;
	newcurr = newas->start;
	kprintf("Test of the copying\n!");
	while (curr != NULL || newcurr != NULL) {
		kprintf("Curr 0x%x == ", curr->vaddress);
		kprintf("newcurr 0x%x \n", newcurr->vaddress);
		curr = curr->next;
		newcurr = newcurr->next;
	}
	kprintf("END\n\n!");


	*ret = newas;
	return 0;
}

void as_destroy(struct addrspace *as) {
	/*
	 * Clean up as needed.
	 */
	if (as == NULL) {
		return;
	}
	if (as->start == NULL) {
		kfree(as);
		return;
	}
	struct as_page* next_p = as->start->next;
	while (next_p != NULL) {
		kfree(as->start);
		as->start = next_p;
		next_p = next_p->next;
	}
	kfree(as->start);
	kfree(as);

}

void as_activate(void) {
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	//dumbvm is best vm
	as_deactivate();
}

void as_deactivate(void) {
	int i, spl;

	spl = splhigh();
	for (i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable) {
	/*
	 * Write this.
	 */
	if (as == NULL) {
		return EINVAL;
	}
	//Thank you so much Kevin, for the dumbvm, it helped us so much to get started
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;


	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	//size_t npages = memsize / PAGE_SIZE;
	mode_t per_mode = 0;
	if (readable) {
		per_mode |= PF_R;
	}
	if (writeable) {
		per_mode |= PF_W;
	}
	if (executable) {
		per_mode |= PF_X;
	}
	struct as_page* curr = as->start;
	struct as_page* oldcurr = as->start;
	while (curr != NULL) {
		oldcurr = curr;
		curr = curr->next;
	}
	curr = kmalloc(sizeof(struct as_page));
	if (curr == NULL) {
		return ENOMEM;
	} 
	curr->vaddress = vaddr;
	curr->size = memsize;
	curr->mode = per_mode;
	curr->next = NULL;
	if (as->start == NULL) {
		as->start = curr;
	}
	else {
		oldcurr->next = curr;
	}
	return 0; 
}

int as_prepare_load(struct addrspace *as) {
	/*
	 * Write this.
	 */
	struct as_page* curr = as->start;
	while (curr != NULL) {
		curr->pre_load_mode = curr->mode;
		curr->mode |= PF_W;
		curr = curr->next;
	}
	return 0;
}

int as_complete_load(struct addrspace *as) {
	struct as_page* curr = as->start;
	while (curr != NULL) {
		curr->mode = curr->pre_load_mode;
		curr = curr->next;
	}
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr) {
	/*
	 * Write this.
	 */
	
	struct as_page* curr = as->start;
	struct as_page* oldcurr = as->start;
	while (curr != NULL) {
		oldcurr = curr;
		curr = curr->next;
	}
	curr = kmalloc(sizeof(struct as_page));
	if (curr == NULL) {
		return ENOMEM;
	}
	curr->vaddress = USERSTACK - STACKPAGES * PAGE_SIZE;
	curr->size = STACKPAGES;
	curr->mode = (PF_R | PF_W | PF_X);
	curr->next = NULL;
	if (as->start == NULL) {
		as->start = curr;
	}
	else {
		oldcurr->next = curr;
	}
	
	/*
	int ret = as_define_region(as, USERSTACK, STACKPAGES*PAGE_SIZE, PF_R, PF_W, PF_X);
	if (ret) {
		return ret;
	}
	*/
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

