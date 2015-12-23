	.data
data1:	.word	10
data2:	.word	20
	.text
main:
	la	$24, data1              0
	la	$25, data2              4,8
	lw	$8, 0($24)              c
	lw	$9, 0($25)              10
	addiu	$21, $0, 1          14
loop1:
	addiu	$4, $4, 1           18
	addu	$10, $10, $8        1c
	and	$11, $10, $8            20
	andi	$12, $10, 0xf0f0    24
	sltiu	$22, $4, 10         28
	beq	$22, $0, loop2          2c
	j	loop1                   30
loop2:
	addiu	$5, $5, 1           34
	nor	$13, $13, $9            38
	or	$14, $13, $9            3c
	sll	$15, $13, 2             40
	srl	$16, $13, 1             44
	sltu	$23, $5, $9         48
	bne	$23, $21, tail          4c
	jal	loop2                   50
tail:
	subu	$17, $5, $4         54
	sw	$10, 4($25)             58
	sw	$11, 8($25)             5c
	sw	$12, 12($25)            60
	sw	$13, 16($25)            64
	sw	$14, 20($25)            68
	sw	$15, 24($25)            6c
	sw	$16, 28($25)            70
