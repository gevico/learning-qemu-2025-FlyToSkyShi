#include "qemu/osdep.h"
#include "hw/ssi/g233_spi.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/irq.h"

static void g233_spi_update_irq(G233SPIState *s)
{
    uint32_t pending = 0;

    /* Check TXE interrupt */
    if ((s->cr2 & SPI_CR2_TXEIE) && (s->sr & SPI_SR_TXE)) {
        pending = 1;
    }

    /* Check RXNE interrupt */
    if ((s->cr2 & SPI_CR2_RXNEIE) && (s->sr & SPI_SR_RXNE)) {
        pending = 1;
    }

    /* Check error interrupts */
    if ((s->cr2 & SPI_CR2_ERRIE) &&
        ((s->sr & SPI_SR_OVERRUN) || (s->sr & SPI_SR_UNDERRUN))) {
        pending = 1;
    }

    qemu_set_irq(s->irq, pending);
}

static void g233_spi_update_cs_lines(G233SPIState *s)
{
    for (int i = 0; i < 4; i++) {
        bool active = false;

        /* CS is active if both enable and active bits are set */
        if ((s->csctrl & (1 << i)) && (s->csctrl & (1 << (i + 4)))) {
            active = true;
        }

        /* CS lines are typically active low */
        qemu_set_irq(s->cs_lines[i], !active);
    }
}

static void g233_spi_transfer(G233SPIState *s)
{
    if (s->sr & SPI_SR_RXNE) {
        s->sr |= SPI_SR_OVERRUN;
        g233_spi_update_irq(s);
        return;
    }

    s->dr = ssi_transfer(s->ssi, s->dr);
    s->sr |= SPI_SR_RXNE;
    s->sr |= SPI_SR_TXE;

    g233_spi_update_irq(s);
}

static uint64_t g233_spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    G233SPIState *s = opaque;
    uint32_t ret = 0;

    switch (addr) {
    case SPI_CR1:
        ret = s->cr1;
        break;

    case SPI_CR2:
        ret = s->cr2;
        break;

    case SPI_SR:
        ret = s->sr;
        break;

    case SPI_DR:
        if((s->sr & SPI_SR_RXNE) == 0) {
            s->sr |= SPI_SR_UNDERRUN;
        } else {
            s->sr &= ~SPI_SR_RXNE;
        }
        g233_spi_update_irq(s);
        ret = s->dr;
        break;

    case SPI_CSCTRL:
        ret = s->csctrl;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "g233_spi: Invalid read offset 0x%" HWADDR_PRIx "\n", addr);
        break;
    }

    return ret;
}

static void g233_spi_write(void *opaque, hwaddr addr, uint64_t value64, unsigned int size)
{
    G233SPIState *s = opaque;
    uint32_t value = value64;
    switch (addr) {
    case SPI_CR1:
        s->cr1 = value;
        break;

    case SPI_CR2:
        s->cr2 = value;
        g233_spi_update_irq(s);
        break;

    case SPI_SR:
        /* Clear error flags by writing 1 */
        if (value & SPI_SR_OVERRUN) {
            s->sr &= ~SPI_SR_OVERRUN;
        }
        if (value & SPI_SR_UNDERRUN) {
            s->sr &= ~SPI_SR_UNDERRUN;
        }
        g233_spi_update_irq(s);
        break;

    case SPI_DR:
        s->dr = value;
        /* Start transfer */
        g233_spi_transfer(s);
        break;

    case SPI_CSCTRL:
        s->csctrl = value;
        g233_spi_update_cs_lines(s);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "g233_spi: Invalid write offset 0x%" HWADDR_PRIx "\n", addr);
        break;
    }
}

static const MemoryRegionOps g233_spi_ops = {
    .read = g233_spi_read,
    .write = g233_spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static void g233_spi_reset(DeviceState *dev)
{
    G233SPIState *s = G233_SPI(dev);

    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = SPI_SR_TXE;
    s->dr = 0x0C;
    s->csctrl = 0;

    g233_spi_update_cs_lines(s);
}

static const VMStateDescription g233_spi_vmstate = {
    .name = TYPE_G233_SPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(cr1, G233SPIState),
        VMSTATE_UINT32(cr2, G233SPIState),
        VMSTATE_UINT32(sr, G233SPIState),
        VMSTATE_UINT32(dr, G233SPIState),
        VMSTATE_UINT32(csctrl, G233SPIState),
        VMSTATE_END_OF_LIST()
    }
};

static void g233_spi_init(Object *obj)
{
    G233SPIState *s = G233_SPI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &g233_spi_ops, s, TYPE_G233_SPI, SPI_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    /* Initialize CS lines */
    for (int i = 0; i < 4; i++) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }
    s->ssi = ssi_create_bus(DEVICE(obj), "ssi");
}

static void g233_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, g233_spi_reset);
    dc->vmsd = &g233_spi_vmstate;
}

static const TypeInfo g233_spi_info = {
    .name = TYPE_G233_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(G233SPIState),
    .instance_init = g233_spi_init,
    .class_init = g233_spi_class_init,
};

static void g233_spi_register_types(void)
{
    type_register_static(&g233_spi_info);
}

type_init(g233_spi_register_types)
