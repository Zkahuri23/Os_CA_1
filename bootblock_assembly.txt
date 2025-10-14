
bootblock:     file format binary


Disassembly of section .data:

00000000 <.data>:
   0:	fa                   	cli
   1:	31 c0                	xor    %ax,%ax
   3:	8e d8                	mov    %ax,%ds
   5:	8e c0                	mov    %ax,%es
   7:	8e d0                	mov    %ax,%ss
   9:	e4 64                	in     $0x64,%al
   b:	a8 02                	test   $0x2,%al
   d:	75 fa                	jne    0x9
   f:	b0 d1                	mov    $0xd1,%al
  11:	e6 64                	out    %al,$0x64
  13:	e4 64                	in     $0x64,%al
  15:	a8 02                	test   $0x2,%al
  17:	75 fa                	jne    0x13
  19:	b0 df                	mov    $0xdf,%al
  1b:	e6 60                	out    %al,$0x60
  1d:	0f 01 16 78 7c       	lgdtw  0x7c78
  22:	0f 20 c0             	mov    %cr0,%eax
  25:	66 83 c8 01          	or     $0x1,%eax
  29:	0f 22 c0             	mov    %eax,%cr0
  2c:	ea 31 7c 08 00       	ljmp   $0x8,$0x7c31
  31:	66 b8 10 00 8e d8    	mov    $0xd88e0010,%eax
  37:	8e c0                	mov    %ax,%es
  39:	8e d0                	mov    %ax,%ss
  3b:	66 b8 00 00 8e e0    	mov    $0xe08e0000,%eax
  41:	8e e8                	mov    %ax,%gs
  43:	bc 00 7c             	mov    $0x7c00,%sp
  46:	00 00                	add    %al,(%bx,%si)
  48:	e8 f0 00             	call   0x13b
  4b:	00 00                	add    %al,(%bx,%si)
  4d:	66 b8 00 8a 66 89    	mov    $0x89668a00,%eax
  53:	c2 66 ef             	ret    $0xef66
  56:	66 b8 e0 8a 66 ef    	mov    $0xef668ae0,%eax
  5c:	eb fe                	jmp    0x5c
  5e:	66 90                	xchg   %eax,%eax
	...
  68:	ff                   	(bad)
  69:	ff 00                	incw   (%bx,%si)
  6b:	00 00                	add    %al,(%bx,%si)
  6d:	9a cf 00 ff ff       	lcall  $0xffff,$0xcf
  72:	00 00                	add    %al,(%bx,%si)
  74:	00 92 cf 00          	add    %dl,0xcf(%bp,%si)
  78:	17                   	pop    %ss
  79:	00 60 7c             	add    %ah,0x7c(%bx,%si)
  7c:	00 00                	add    %al,(%bx,%si)
  7e:	ba f7 01             	mov    $0x1f7,%dx
  81:	00 00                	add    %al,(%bx,%si)
  83:	ec                   	in     (%dx),%al
  84:	83 e0 c0             	and    $0xffc0,%ax
  87:	3c 40                	cmp    $0x40,%al
  89:	75 f8                	jne    0x83
  8b:	c3                   	ret
  8c:	55                   	push   %bp
  8d:	89 e5                	mov    %sp,%bp
  8f:	57                   	push   %di
  90:	53                   	push   %bx
  91:	8b 5d 0c             	mov    0xc(%di),%bx
  94:	e8 e5 ff             	call   0x7c
  97:	ff                   	(bad)
  98:	ff                   	(bad)
  99:	b8 01 00             	mov    $0x1,%ax
  9c:	00 00                	add    %al,(%bx,%si)
  9e:	ba f2 01             	mov    $0x1f2,%dx
  a1:	00 00                	add    %al,(%bx,%si)
  a3:	ee                   	out    %al,(%dx)
  a4:	ba f3 01             	mov    $0x1f3,%dx
  a7:	00 00                	add    %al,(%bx,%si)
  a9:	89 d8                	mov    %bx,%ax
  ab:	ee                   	out    %al,(%dx)
  ac:	89 d8                	mov    %bx,%ax
  ae:	c1 e8 08             	shr    $0x8,%ax
  b1:	ba f4 01             	mov    $0x1f4,%dx
  b4:	00 00                	add    %al,(%bx,%si)
  b6:	ee                   	out    %al,(%dx)
  b7:	89 d8                	mov    %bx,%ax
  b9:	c1 e8 10             	shr    $0x10,%ax
  bc:	ba f5 01             	mov    $0x1f5,%dx
  bf:	00 00                	add    %al,(%bx,%si)
  c1:	ee                   	out    %al,(%dx)
  c2:	89 d8                	mov    %bx,%ax
  c4:	c1 e8 18             	shr    $0x18,%ax
  c7:	83 c8 e0             	or     $0xffe0,%ax
  ca:	ba f6 01             	mov    $0x1f6,%dx
  cd:	00 00                	add    %al,(%bx,%si)
  cf:	ee                   	out    %al,(%dx)
  d0:	b8 20 00             	mov    $0x20,%ax
  d3:	00 00                	add    %al,(%bx,%si)
  d5:	ba f7 01             	mov    $0x1f7,%dx
  d8:	00 00                	add    %al,(%bx,%si)
  da:	ee                   	out    %al,(%dx)
  db:	e8 9e ff             	call   0x7c
  de:	ff                   	(bad)
  df:	ff 8b 7d 08          	decw   0x87d(%bp,%di)
  e3:	b9 80 00             	mov    $0x80,%cx
  e6:	00 00                	add    %al,(%bx,%si)
  e8:	ba f0 01             	mov    $0x1f0,%dx
  eb:	00 00                	add    %al,(%bx,%si)
  ed:	fc                   	cld
  ee:	f3 6d                	rep insw (%dx),%es:(%di)
  f0:	5b                   	pop    %bx
  f1:	5f                   	pop    %di
  f2:	5d                   	pop    %bp
  f3:	c3                   	ret
  f4:	55                   	push   %bp
  f5:	89 e5                	mov    %sp,%bp
  f7:	57                   	push   %di
  f8:	56                   	push   %si
  f9:	53                   	push   %bx
  fa:	83 ec 0c             	sub    $0xc,%sp
  fd:	8b 5d 08             	mov    0x8(%di),%bx
 100:	8b 75 10             	mov    0x10(%di),%si
 103:	89 df                	mov    %bx,%di
 105:	03 7d 0c             	add    0xc(%di),%di
 108:	89 f0                	mov    %si,%ax
 10a:	25 ff 01             	and    $0x1ff,%ax
 10d:	00 00                	add    %al,(%bx,%si)
 10f:	29 c3                	sub    %ax,%bx
 111:	c1 ee 09             	shr    $0x9,%si
 114:	83 c6 01             	add    $0x1,%si
 117:	39 fb                	cmp    %di,%bx
 119:	73 1a                	jae    0x135
 11b:	83 ec 08             	sub    $0x8,%sp
 11e:	56                   	push   %si
 11f:	53                   	push   %bx
 120:	e8 67 ff             	call   0x8a
 123:	ff                   	(bad)
 124:	ff 81 c3 00          	incw   0xc3(%bx,%di)
 128:	02 00                	add    (%bx,%si),%al
 12a:	00 83 c6 01          	add    %al,0x1c6(%bp,%di)
 12e:	83 c4 10             	add    $0x10,%sp
 131:	39 fb                	cmp    %di,%bx
 133:	72 e6                	jb     0x11b
 135:	8d 65 f4             	lea    -0xc(%di),%sp
 138:	5b                   	pop    %bx
 139:	5e                   	pop    %si
 13a:	5f                   	pop    %di
 13b:	5d                   	pop    %bp
 13c:	c3                   	ret
 13d:	55                   	push   %bp
 13e:	89 e5                	mov    %sp,%bp
 140:	57                   	push   %di
 141:	56                   	push   %si
 142:	53                   	push   %bx
 143:	83 ec 10             	sub    $0x10,%sp
 146:	6a 00                	push   $0x0
 148:	68 00 10             	push   $0x1000
 14b:	00 00                	add    %al,(%bx,%si)
 14d:	68 00 00             	push   $0x0
 150:	01 00                	add    %ax,(%bx,%si)
 152:	e8 9d ff             	call   0xf2
 155:	ff                   	(bad)
 156:	ff 83 c4 10          	incw   0x10c4(%bp,%di)
 15a:	81 3d 00 00          	cmpw   $0x0,(%di)
 15e:	01 00                	add    %ax,(%bx,%si)
 160:	7f 45                	jg     0x1a7
 162:	4c                   	dec    %sp
 163:	46                   	inc    %si
 164:	75 21                	jne    0x187
 166:	a1 1c 00             	mov    0x1c,%ax
 169:	01 00                	add    %ax,(%bx,%si)
 16b:	8d 98 00 00          	lea    0x0(%bx,%si),%bx
 16f:	01 00                	add    %ax,(%bx,%si)
 171:	0f b7 35             	movzww (%di),%si
 174:	2c 00                	sub    $0x0,%al
 176:	01 00                	add    %ax,(%bx,%si)
 178:	c1 e6 05             	shl    $0x5,%si
 17b:	01 de                	add    %bx,%si
 17d:	39 f3                	cmp    %si,%bx
 17f:	72 15                	jb     0x196
 181:	ff 15                	call   *(%di)
 183:	18 00                	sbb    %al,(%bx,%si)
 185:	01 00                	add    %ax,(%bx,%si)
 187:	8d 65 f4             	lea    -0xc(%di),%sp
 18a:	5b                   	pop    %bx
 18b:	5e                   	pop    %si
 18c:	5f                   	pop    %di
 18d:	5d                   	pop    %bp
 18e:	c3                   	ret
 18f:	83 c3 20             	add    $0x20,%bx
 192:	39 f3                	cmp    %si,%bx
 194:	73 eb                	jae    0x181
 196:	8b 7b 0c             	mov    0xc(%bp,%di),%di
 199:	83 ec 04             	sub    $0x4,%sp
 19c:	ff 73 04             	push   0x4(%bp,%di)
 19f:	ff 73 10             	push   0x10(%bp,%di)
 1a2:	57                   	push   %di
 1a3:	e8 4c ff             	call   0xf2
 1a6:	ff                   	(bad)
 1a7:	ff 8b 4b 14          	decw   0x144b(%bp,%di)
 1ab:	8b 43 10             	mov    0x10(%bp,%di),%ax
 1ae:	83 c4 10             	add    $0x10,%sp
 1b1:	39 c8                	cmp    %cx,%ax
 1b3:	73 da                	jae    0x18f
 1b5:	01 c7                	add    %ax,%di
 1b7:	29 c1                	sub    %ax,%cx
 1b9:	b8 00 00             	mov    $0x0,%ax
 1bc:	00 00                	add    %al,(%bx,%si)
 1be:	fc                   	cld
 1bf:	f3 aa                	rep stos %al,%es:(%di)
 1c1:	eb cc                	jmp    0x18f
	...
 1fb:	00 00                	add    %al,(%bx,%si)
 1fd:	00 55 aa             	add    %dl,-0x56(%di)
