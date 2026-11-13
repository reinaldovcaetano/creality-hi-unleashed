static void LCD_panel_init(u32 sel)
{
	sunxi_lcd_gpio_set_direction(0, 0, 1);
	sunxi_lcd_gpio_set_direction(0, 2, 1);
	sunxi_lcd_gpio_set_direction(0, 1, 1);
	sunxi_lcd_gpio_set_direction(0, 3, 1);
	usleep_range(5000, 5000);
	sunxi_lcd_gpio_set_value(0, 0, 1);
	sunxi_lcd_gpio_set_value(0, 2, 0);
	sunxi_lcd_gpio_set_value(0, 1, 0);
	sunxi_lcd_gpio_set_value(0, 3, 1);
	/* printk @('lit', 3231420661) */
	usleep_range(50000, 50000);
	gc9307_9bit_write(0x0fe);	/* comando */
	gc9307_9bit_write(0x0ef);	/* comando */
	gc9307_9bit_write(0x036);	/* comando */
	gc9307_9bit_write(0x140);
	gc9307_9bit_write(0x03a);	/* comando */
	gc9307_9bit_write(0x160);
	gc9307_9bit_write(0x084);	/* comando */
	gc9307_9bit_write(0x141);
	gc9307_9bit_write(0x08a);	/* comando */
	gc9307_9bit_write(0x140);
	gc9307_9bit_write(0x0b0);	/* comando */
	gc9307_9bit_write(0x140);
	gc9307_9bit_write(0x0f6);	/* comando */
	gc9307_9bit_write(0x1c6);
	gc9307_9bit_write(0x0b5);	/* comando */
	gc9307_9bit_write(0x108);
	gc9307_9bit_write(0x102);
	gc9307_9bit_write(0x114);
	gc9307_9bit_write(0x086);	/* comando */
	gc9307_9bit_write(0x198);
	gc9307_9bit_write(0x089);	/* comando */
	gc9307_9bit_write(0x113);
	gc9307_9bit_write(0x08b);	/* comando */
	gc9307_9bit_write(0x180);
	gc9307_9bit_write(0x08d);	/* comando */
	gc9307_9bit_write(0x133);
	gc9307_9bit_write(0x08e);	/* comando */
	gc9307_9bit_write(0x10f);
	gc9307_9bit_write(0x0e8);	/* comando */
	gc9307_9bit_write(0x112);
	gc9307_9bit_write(0x162);
	gc9307_9bit_write(0x0ec);	/* comando */
	gc9307_9bit_write(0x133);
	gc9307_9bit_write(0x101);
	gc9307_9bit_write(0x140);
	gc9307_9bit_write(0x0ff);	/* comando */
	gc9307_9bit_write(0x162);
	gc9307_9bit_write(0x099);	/* comando */
	gc9307_9bit_write(0x13e);
	gc9307_9bit_write(0x09d);	/* comando */
	gc9307_9bit_write(0x14b);
	gc9307_9bit_write(0x098);	/* comando */
	gc9307_9bit_write(0x13e);
	gc9307_9bit_write(0x09c);	/* comando */
	gc9307_9bit_write(0x14b);
	gc9307_9bit_write(0x0c3);	/* comando */
	gc9307_9bit_write(0x105);
	gc9307_9bit_write(0x0c4);	/* comando */
	gc9307_9bit_write(0x115);
	gc9307_9bit_write(0x0c9);	/* comando */
	gc9307_9bit_write(0x104);
	gc9307_9bit_write(0x0f0);	/* comando */
	gc9307_9bit_write(0x116);
	gc9307_9bit_write(0x117);
	gc9307_9bit_write(0x109);
	gc9307_9bit_write(0x106);
	gc9307_9bit_write(0x1f3);
	gc9307_9bit_write(0x125);
	gc9307_9bit_write(0x0f1);	/* comando */
	gc9307_9bit_write(0x13b);
	gc9307_9bit_write(0x19b);
	gc9307_9bit_write(0x19a);
	gc9307_9bit_write(0x121);
	gc9307_9bit_write(0x123);
	gc9307_9bit_write(0x1df);
	gc9307_9bit_write(0x0f2);	/* comando */
	gc9307_9bit_write(0x116);
	gc9307_9bit_write(0x117);
	gc9307_9bit_write(0x109);
	gc9307_9bit_write(0x106);
	gc9307_9bit_write(0x1f3);
	gc9307_9bit_write(0x125);
	gc9307_9bit_write(0x0f3);	/* comando */
	gc9307_9bit_write(0x13b);
	gc9307_9bit_write(0x19b);
	gc9307_9bit_write(0x19a);
	gc9307_9bit_write(0x121);
	gc9307_9bit_write(0x123);
	gc9307_9bit_write(0x1df);
	gc9307_9bit_write(0x035);	/* comando */
	gc9307_9bit_write(0x100);
	gc9307_9bit_write(0x044);	/* comando */
	gc9307_9bit_write(0x100);
	gc9307_9bit_write(0x10a);
	gc9307_9bit_write(0x011);	/* comando */
	msleep(120);
	gc9307_9bit_write(0x029);	/* comando */
	gc9307_9bit_write(0x02c);	/* comando */
	/* printk @('lit', 3231420681) */
}
