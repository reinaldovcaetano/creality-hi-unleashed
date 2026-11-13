# Projeto Hi: i2c-sunxi v2.6.5 -> nova tentativa quando o escravo não responde (NACK/erro de barramento).
# Motivo: no kernel próprio (13/11/2026) a 1a leitura da EEPROM bl24c16f (0x50, i2c.1) levou NACK e o
# Klipper (MCU 'rpi') entrou em shutdown. O driver da Creality (v2.1.4) nunca registrou esse erro.
import sys
p = sys.argv[1]; s = open(p).read()
old = """	if (twi->twi_drv_used)
		ret = sunxi_twi_drv_xfer(twi, msgs, num);
	else
		ret = sunxi_twi_engine_xfer(twi, msgs, num);

out:"""
new = """	{
		/* Ajuste Projeto Hi: até 3 novas tentativas, 5 ms entre elas (EEPROM ocupada / barramento recém-acordado) */
		int tentativa;
		for (tentativa = 0; tentativa <= 3; tentativa++) {
			if (tentativa) {
				sunxi_err(twi->dev, "hi: nova tentativa %d (dev addr:0x%x, erro %d)\\n",
					  tentativa, msgs[0].addr, ret);
				msleep(5);
				sunxi_twi_soft_reset(twi);
				if (sunxi_twi_bus_barrier(&twi->adap))
					continue;
			}
			if (twi->twi_drv_used)
				ret = sunxi_twi_drv_xfer(twi, msgs, num);
			else
				ret = sunxi_twi_engine_xfer(twi, msgs, num);
			if (ret >= 0)
				break;
		}
		if (tentativa && ret >= 0)
			sunxi_err(twi->dev, "hi: recuperado na tentativa %d (dev addr:0x%x)\\n", tentativa, msgs[0].addr);
	}

out:"""
assert s.count(old) == 1, "trecho não encontrado"
s = s.replace(old, new)
if "#include <linux/delay.h>" not in s:
    s = s.replace("#include <linux/dma-mapping.h>", "#include <linux/dma-mapping.h>\n#include <linux/delay.h>", 1)
s = s.replace('#define SUNXI_TWI_VERSION\t"2.6.5"', '#define SUNXI_TWI_VERSION\t"2.6.5-hi1"')
open(p, "w").write(s); print("ok")
