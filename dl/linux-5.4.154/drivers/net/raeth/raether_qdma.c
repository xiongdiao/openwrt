#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/if_ether.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/mach-ralink/surfboardint.h>
#if defined (CONFIG_RAETH_TSO)
#include <linux/tcp.h>
#include <net/ipv6.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <linux/in.h>
#include <linux/ppp_defs.h>
#include <linux/if_pppox.h>
#endif
#include <linux/delay.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
#include <linux/sched.h>
#endif
#if defined (CONFIG_HW_SFQ)
#include <linux/if_vlan.h>
#include <net/ipv6.h>
#include <net/ip.h>
#include <linux/if_pppox.h>
#include <linux/ppp_defs.h>
#endif
#if defined(CONFIG_ARCH_MT7622)
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#if !defined (CONFIG_ARCH_MT7622)
#include <asm/mach-ralink/rt_mmap.h>
#endif
#else
#include <linux/libata-compat.h>
#endif
 
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,20)
#include <asm-generic/pci-dma-compat.h>
#endif

#include "ra2882ethreg.h"
#include "raether.h"
#include "ra_mac.h"
#include "ra_ioctl.h"
#include "ra_rfrw.h"
#ifdef CONFIG_RAETH_NETLINK
#include "ra_netlink.h"
#endif
#if defined (CONFIG_RAETH_QOS)
#include "ra_qos.h"
#endif

#if defined (CONFIG_HW_SFQ) || defined (CONFIG_RA_HW_NAT) || defined (CONFIG_RA_HW_NAT_MODULE)
#include "../../../net/nat/hw_nat/ra_nat.h"
#endif

#if !defined(CONFIG_RA_NAT_NONE)
/* bruce+
 */
extern int (*ra_sw_nat_hook_rx)(struct sk_buff *skb);
extern int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_no);
#endif

#if defined(CONFIG_RA_CLASSIFIER)||defined(CONFIG_RA_CLASSIFIER_MODULE)
/* Qwert+
 */
#include <asm/mipsregs.h>
extern int (*ra_classifier_hook_tx)(struct sk_buff *skb, unsigned long cur_cycle);
extern int (*ra_classifier_hook_rx)(struct sk_buff *skb, unsigned long cur_cycle);
#endif /* CONFIG_RA_CLASSIFIER */

#if defined (CONFIG_RALINK_RT3052_MP2)
int32_t mcast_rx(struct sk_buff * skb);
int32_t mcast_tx(struct sk_buff * skb);
#endif

#ifdef RA_MTD_RW_BY_NUM
int ra_mtd_read(int num, loff_t from, size_t len, u_char *buf);
#else
int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf);
#endif

/* gmac driver feature set config */
#if defined (CONFIG_RAETH_NAPI) || defined (CONFIG_RAETH_QOS)
#undef DELAY_INT
#else
#if defined     (CONFIG_ARCH_MT7623)
#define DELAY_INT       1
#else
#define DELAY_INT	1
#endif
#endif

//#define CONFIG_UNH_TEST
/* end of config */

#if defined (CONFIG_RAETH_JUMBOFRAME)
#define	MAX_RX_LENGTH	4096
#else
#define	MAX_RX_LENGTH	1536
#endif

extern struct net_device		*dev_raether;

#if defined (CONFIG_RAETH_MULTIPLE_RX_RING)
static int rx_dma_owner_idx1;
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
static int rx_calc_idx1;
#endif
#endif
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
static int rx_calc_idx0;
#endif
extern unsigned long tx_ring_full;

#if defined (CONFIG_ETHTOOL) && defined (CONFIG_RAETH_ROUTER)
#include "ra_ethtool.h"
extern struct ethtool_ops	ra_ethtool_ops;
#ifdef CONFIG_PSEUDO_SUPPORT
extern struct ethtool_ops	ra_virt_ethtool_ops;
#endif // CONFIG_PSEUDO_SUPPORT //
#endif // (CONFIG_ETHTOOL //

#ifdef CONFIG_RALINK_VISTA_BASIC
int is_switch_175c = 1;
#endif

//skb->mark to queue mapping table
extern unsigned int M2Q_table[64];
struct QDMA_txdesc *free_head = NULL;
extern unsigned int lan_wan_separate;
#if defined (CONFIG_HW_SFQ)
extern unsigned int web_sfq_enable;
#define HwSfqQUp 3
#define HwSfqQDl 1
#endif
int dbg =0;//debug used
int sfq_debug = 0;
#if defined (CONFIG_HW_SFQ)
struct SFQ_table *sfq0;
struct SFQ_table *sfq1;
struct SFQ_table *sfq2;
struct SFQ_table *sfq3;
#endif

#define KSEG1                   0xa0000000
#if defined (CONFIG_MIPS)
#define PHYS_TO_VIRT(x)         ((void *)((x) | KSEG1))
#define VIRT_TO_PHYS(x)         ((unsigned long)(x) & ~KSEG1)
#else
#define PHYS_TO_VIRT(x)         phys_to_virt(x)
#define VIRT_TO_PHYS(x)         virt_to_phys(x)
#endif

extern void set_fe_dma_glo_cfg(void);

#if defined (CONFIG_HW_SFQ)
ParseResult		SfqParseResult;
#endif

/**
 *
 * @brief: get the TXD index from its address
 *
 * @param: cpu_ptr
 *
 * @return: TXD index
*/

static inline unsigned int GET_TXD_OFFSET(END_DEVICE *ei_local, struct QDMA_txdesc **cpu_ptr)
{
	int ctx_offset;
	ctx_offset = ((u32)*cpu_ptr - (u32)ei_local->phy_txd_pool)/sizeof(struct QDMA_txdesc);

  	return ctx_offset;
} 




/**
 * @brief cal txd number for a page
 *
 * @parm size
 *
 * @return frag_txd_num
 */

static inline unsigned int cal_frag_txd_num(unsigned int size)
{
	unsigned int frag_txd_num = 0;
	if(size == 0)
		return 0;
	while(size > 0){
		if(size > MAX_TXD_LEN){
			frag_txd_num++;
			size -= MAX_TXD_LEN;
		}else{
			frag_txd_num++;
		        size = 0;
		}
	}
        return frag_txd_num;

}

/**
 * @brief get free TXD from TXD queue
 *
 * @param free_txd
 *
 * @return 
 */
static inline int get_free_txd(END_DEVICE *ei_local, struct QDMA_txdesc **free_txd)
{
	unsigned int tmp_idx;

	tmp_idx = ei_local->free_txd_head;
	ei_local->free_txd_head = ei_local->txd_pool_info[tmp_idx];
	atomic_sub(1, &ei_local->free_txd_num);
	//*free_txd = &ei_local->txd_pool[tmp_idx];
	*free_txd = ( struct QDMA_txdesc *)(ei_local->phy_txd_pool + (sizeof(struct QDMA_txdesc) * tmp_idx));
	return tmp_idx;
}


/**
 * @brief add free TXD into TXD queue
 *
 * @param free_txd
 *
 * @return 
 */
static inline void put_free_txd(END_DEVICE *ei_local, int free_txd_idx)
{
	ei_local->txd_pool_info[ei_local->free_txd_tail] = free_txd_idx;
	ei_local->free_txd_tail = free_txd_idx;
}

/*define qdma initial alloc*/
/**
 * @brief 
 *
 * @param net_dev
 *
 * @return  0: fail
 *	    1: success
 */
bool qdma_tx_desc_alloc(void)
{
	struct net_device *dev = dev_raether;
	END_DEVICE *ei_local = netdev_priv(dev);
	struct QDMA_txdesc *free_txd = NULL;
	int i = 0;

	
#if defined (CONFIG_MIPS)
	ei_local->txd_pool = pci_alloc_consistent(NULL, sizeof(struct QDMA_txdesc) * NUM_TX_DESC, &ei_local->phy_txd_pool);
#else
	ei_local->txd_pool = dma_alloc_coherent(dev->dev.parent, sizeof(struct QDMA_txdesc) * NUM_TX_DESC, &ei_local->phy_txd_pool, GFP_KERNEL);
#endif 
	printk("txd_pool=%p phy_txd_pool=%08X\n", ei_local->txd_pool , ei_local->phy_txd_pool);

	if (ei_local->txd_pool == NULL) {
		printk("adapter->txd_pool allocation failed!\n");
		return 0;
	}
	printk("ei_local->skb_free start address is 0x%p.\n", ei_local->skb_free);
	//set all txd_pool_info to 0.
	for ( i = 0; i < NUM_TX_DESC; i++)
	{
		ei_local->skb_free[i]= 0;
		ei_local->txd_pool_info[i] = i + 1;
		ei_local->txd_pool[i].txd_info3.LS_bit = 1;
		ei_local->txd_pool[i].txd_info3.OWN_bit = 1;
	}

	ei_local->free_txd_head = 0;
	ei_local->free_txd_tail = NUM_TX_DESC - 1;
	atomic_set(&ei_local->free_txd_num, NUM_TX_DESC);
	

	//get free txd from txd pool
	ei_local->tx_cpu_owner_idx0 = get_free_txd(ei_local, &free_txd);
	
	//add null TXD for transmit
	//ei_local->tx_dma_ptr = VIRT_TO_PHYS(free_txd);
	//ei_local->tx_cpu_ptr = VIRT_TO_PHYS(free_txd);
	ei_local->tx_dma_ptr = (unsigned int)free_txd;
	ei_local->tx_cpu_ptr = (unsigned int)free_txd;
	sysRegWrite(QTX_CTX_PTR, ei_local->tx_cpu_ptr);
	sysRegWrite(QTX_DTX_PTR, ei_local->tx_dma_ptr);
	
	//get free txd from txd pool

	ei_local->tx_cpu_release_idx0 = get_free_txd(ei_local, &free_txd);
	// add null TXD for release
	//sysRegWrite(QTX_CRX_PTR, VIRT_TO_PHYS(free_txd));
	//sysRegWrite(QTX_DRX_PTR, VIRT_TO_PHYS(free_txd));
	/*Reserve 4 TXD for each physical queue*/
#if defined (CONFIG_ARCH_MT7623) || defined (CONFIG_RALINK_MT7621)
	for (i = 0; i< NUM_PQ; i++)
		sysRegWrite(QTX_CFG_0 + QUEUE_OFFSET * i, (NUM_PQ_RESV | (NUM_PQ_RESV << 8)));
#endif	
#if defined (CONFIG_ARCH_MT7622)
	for (i = 0; i< NUM_PQ; i++){
		if(i <= 15){ 
			sysRegWrite(QDMA_PAGE, 0); 
			sysRegWrite(QTX_CFG_0 + QUEUE_OFFSET * i, (NUM_PQ_RESV | (NUM_PQ_RESV << 8)));
		}else if (i > 15 && i <= 31){
			sysRegWrite(QDMA_PAGE, 1); 
			sysRegWrite(QTX_CFG_0 + QUEUE_OFFSET * (i - 16), (NUM_PQ_RESV | (NUM_PQ_RESV << 8)));	
		}else if (i > 31 && i <= 47){
			sysRegWrite(QDMA_PAGE, 2); 
			sysRegWrite(QTX_CFG_0 + QUEUE_OFFSET * (i - 32), (NUM_PQ_RESV | (NUM_PQ_RESV << 8)));	
		}else if (i > 47 && i <= 63){
			sysRegWrite(QDMA_PAGE, 3); 
			sysRegWrite(QTX_CFG_0 + QUEUE_OFFSET * (i - 48), (NUM_PQ_RESV | (NUM_PQ_RESV << 8)));	
		}
	}
#endif	
	sysRegWrite(QTX_CRX_PTR, (unsigned int)free_txd);
	sysRegWrite(QTX_DRX_PTR, (unsigned int)free_txd);
	printk("free_txd: %p, ei_local->cpu_ptr: %08X\n", free_txd, ei_local->tx_cpu_ptr);
	
	printk(" POOL  HEAD_PTR | DMA_PTR | CPU_PTR \n");
	printk("----------------+---------+--------\n");
	printk("     0x%p 0x%08X 0x%08X\n",ei_local->txd_pool, ei_local->tx_dma_ptr, ei_local->tx_cpu_ptr);
	return 1;
}
#if defined (CONFIG_HW_SFQ)
bool sfq_init(struct net_device *dev)
{
	unsigned int regVal;
	dma_addr_t sfq_phy0;
	dma_addr_t sfq_phy1;
	dma_addr_t sfq_phy2;
	dma_addr_t sfq_phy3;	
  	struct SFQ_table *sfq0;
	struct SFQ_table *sfq1;
	struct SFQ_table *sfq2;
	struct SFQ_table *sfq3;
#if defined (CONFIG_ARCH_MT7622)
	dma_addr_t sfq_phy4;
	dma_addr_t sfq_phy5;
	dma_addr_t sfq_phy6;
	dma_addr_t sfq_phy7;	
  	struct SFQ_table *sfq4;
	struct SFQ_table *sfq5;
	struct SFQ_table *sfq6;
	struct SFQ_table *sfq7;
#endif
	int i = 0;
	regVal = sysRegRead(VQTX_GLO);
	regVal = regVal | VQTX_MIB_EN;
	sysRegWrite(VQTX_GLO, regVal);// Virtual table extends to 32bytes
	regVal = sysRegRead(VQTX_GLO);
#if defined (CONFIG_ARCH_MT7622)
	sysRegWrite(VQTX_NUM, (VQTX_NUM_0) | (VQTX_NUM_1) | (VQTX_NUM_2) | (VQTX_NUM_3) | (VQTX_NUM_4) | (VQTX_NUM_5) | (VQTX_NUM_6) | (VQTX_NUM_7));
#else
	sysRegWrite(VQTX_NUM, (VQTX_NUM_0) | (VQTX_NUM_1) | (VQTX_NUM_2) | (VQTX_NUM_3));
#endif

	sysRegWrite(VQTX_HASH_CFG, 0xF002710); //10 s change hash algorithm
	
#if defined (CONFIG_ARCH_MT7622)
	sysRegWrite(VQTX_VLD_CFG, 0xeca86420);	
#else
	sysRegWrite(VQTX_VLD_CFG, 0xc840);
#endif
	sysRegWrite(VQTX_HASH_SD, 0x0D);
	sysRegWrite(QDMA_FC_THRES, 0x9b9b4444);
	sysRegWrite(QDMA_HRED1, 0);
	sysRegWrite(QDMA_HRED2, 0);
	sysRegWrite(QDMA_SRED1, 0);
	sysRegWrite(QDMA_SRED2, 0);
#if defined (CONFIG_ARCH_MT7622)
	sysRegWrite(VQTX_0_3_BIND_QID, (VQTX_0_BIND_QID) | (VQTX_1_BIND_QID) | (VQTX_2_BIND_QID) | (VQTX_3_BIND_QID) );
	sysRegWrite(VQTX_4_7_BIND_QID, (VQTX_4_BIND_QID) | (VQTX_5_BIND_QID) | (VQTX_6_BIND_QID) | (VQTX_7_BIND_QID) );
	printk("(VQTX_0_BIND_QID) | (VQTX_1_BIND_QID) | (VQTX_2_BIND_QID) | (VQTX_3_BIND_QID) =%x\n", (VQTX_0_BIND_QID) | (VQTX_1_BIND_QID) | (VQTX_2_BIND_QID) | (VQTX_3_BIND_QID));
	printk("VQTX_0_3_BIND_QID =%x\n", sysRegRead(VQTX_0_3_BIND_QID));
	printk("(VQTX_4_BIND_QID) | (VQTX_5_BIND_QID) | (VQTX_6_BIND_QID) | (VQTX_7_BIND_QID) =%x\n", (VQTX_4_BIND_QID) | (VQTX_5_BIND_QID) | (VQTX_6_BIND_QID) | (VQTX_7_BIND_QID));
	printk("VQTX_4_7_BIND_QID =%x\n", sysRegRead(VQTX_4_7_BIND_QID));
#endif

#if defined (CONFIG_MIPS)
	sfq0 = pci_alloc_consistent(NULL, VQTX_NUM0*sizeof(struct SFQ_table), &sfq_phy0);
#else
	sfq0 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM0*sizeof(struct SFQ_table), &sfq_phy0, GFP_KERNEL);
#endif
 	
	memset(sfq0, 0x0, VQTX_NUM0*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM0; i++) {
		sfq0[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq0[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}

#if defined (CONFIG_MIPS)
	sfq1 = pci_alloc_consistent(NULL, VQTX_NUM1*sizeof(struct SFQ_table), &sfq_phy1);
#else
	sfq1 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM1*sizeof(struct SFQ_table), &sfq_phy1, GFP_KERNEL);
#endif
	memset(sfq1, 0x0, VQTX_NUM1*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM1; i++) {
		sfq1[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq1[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}
#if defined (CONFIG_MIPS)	
	sfq2 = pci_alloc_consistent(NULL, VQTX_NUM2*sizeof(struct SFQ_table), &sfq_phy2);
#else
	sfq2 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM2*sizeof(struct SFQ_table), &sfq_phy2, GFP_KERNEL);
#endif
	memset(sfq2, 0x0, VQTX_NUM2*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM2; i++) {
		sfq2[i].sfq_info1.VQHPTR = 0xdeadbeef;
      		sfq2[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}
#if defined (CONFIG_MIPS)
	sfq3 = pci_alloc_consistent(NULL, VQTX_NUM3*sizeof(struct SFQ_table), &sfq_phy3);
#else
	sfq3 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM3*sizeof(struct SFQ_table), &sfq_phy3, GFP_KERNEL);
#endif
	memset(sfq3, 0x0, VQTX_NUM3*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM3; i++) {
		sfq3[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq3[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}
	if (unlikely((sfq0 == NULL)) || unlikely((sfq1 == NULL)) || unlikely((sfq2 == NULL)) || unlikely((sfq3 == NULL))){
		printk(KERN_ERR "QDMA SFQ0~3 VQ not available...\n");
		return 1;
	}

#if defined (CONFIG_ARCH_MT7622)
	sfq4 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM4*sizeof(struct SFQ_table), &sfq_phy4, GFP_KERNEL);
	memset(sfq4, 0x0, VQTX_NUM4*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM4; i++) {
		sfq4[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq4[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}
	sfq5 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM5*sizeof(struct SFQ_table), &sfq_phy5, GFP_KERNEL);
	memset(sfq5, 0x0, VQTX_NUM5*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM5; i++) {
		sfq5[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq5[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}	
	sfq6 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM6*sizeof(struct SFQ_table), &sfq_phy6, GFP_KERNEL);
	memset(sfq6, 0x0, VQTX_NUM6*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM6; i++) {
		sfq6[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq6[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}	
	sfq7 = dma_alloc_coherent(dev->dev.parent, VQTX_NUM7*sizeof(struct SFQ_table), &sfq_phy7, GFP_KERNEL);
	memset(sfq7, 0x0, VQTX_NUM7*sizeof(struct SFQ_table) );
	for (i=0; i < VQTX_NUM7; i++) {
		sfq7[i].sfq_info1.VQHPTR = 0xdeadbeef;
		sfq7[i].sfq_info2.VQTPTR = 0xdeadbeef;
	}
	if (unlikely((sfq4 == NULL)) || unlikely((sfq5 == NULL)) || unlikely((sfq6 == NULL)) || unlikely((sfq7 == NULL))){
		printk(KERN_ERR "QDMA SFQ4~7 VQ not available...\n");
		return 1;
	}
#endif


	printk("*****sfq_phy0 is 0x%x!!!*******\n", sfq_phy0);
	printk("*****sfq_phy1 is 0x%x!!!*******\n", sfq_phy1);
	printk("*****sfq_phy2 is 0x%x!!!*******\n", sfq_phy2);
	printk("*****sfq_phy3 is 0x%x!!!*******\n", sfq_phy3);
	printk("*****sfq_virt0 is 0x%p!!!*******\n", sfq0);
	printk("*****sfq_virt1 is 0x%p!!!*******\n", sfq1);
	printk("*****sfq_virt2 is 0x%p!!!*******\n", sfq2);
	printk("*****sfq_virt3 is 0x%p!!!*******\n", sfq3);
#if defined (CONFIG_ARCH_MT7622)
	printk("*****sfq_phy4 is 0x%x!!!*******\n", sfq_phy4);
	printk("*****sfq_phy5 is 0x%x!!!*******\n", sfq_phy5);
	printk("*****sfq_phy6 is 0x%x!!!*******\n", sfq_phy6);
	printk("*****sfq_phy7 is 0x%x!!!*******\n", sfq_phy7);
	printk("*****sfq_virt4 is 0x%p!!!*******\n", sfq4);
	printk("*****sfq_virt5 is 0x%p!!!*******\n", sfq5);
	printk("*****sfq_virt6 is 0x%p!!!*******\n", sfq6);
	printk("*****sfq_virt7 is 0x%p!!!*******\n", sfq7);
#endif
	sysRegWrite(VQTX_TB_BASE0, (u32)sfq_phy0);
	sysRegWrite(VQTX_TB_BASE1, (u32)sfq_phy1);
	sysRegWrite(VQTX_TB_BASE2, (u32)sfq_phy2);
	sysRegWrite(VQTX_TB_BASE3, (u32)sfq_phy3);
#if defined (CONFIG_ARCH_MT7622)
	sysRegWrite(VQTX_TB_BASE4, (u32)sfq_phy4);
	sysRegWrite(VQTX_TB_BASE5, (u32)sfq_phy5);
	sysRegWrite(VQTX_TB_BASE6, (u32)sfq_phy6);
	sysRegWrite(VQTX_TB_BASE7, (u32)sfq_phy7);
#endif

	return 0;
}
#endif
bool fq_qdma_init(struct net_device *dev)
{
	END_DEVICE* ei_local = netdev_priv(dev);
	//struct QDMA_txdesc *free_head = NULL;
	unsigned int phy_free_head;
	unsigned int phy_free_tail;
	unsigned int *free_page_head = NULL;
	unsigned int phy_free_page_head;
	int i;
    
#if defined (CONFIG_MIPS)
	free_head = pci_alloc_consistent(NULL, NUM_QDMA_PAGE * sizeof(struct QDMA_txdesc), &phy_free_head);
#else
	free_head = dma_alloc_coherent(dev->dev.parent, NUM_QDMA_PAGE * sizeof(struct QDMA_txdesc), &phy_free_head, GFP_KERNEL);
#endif    
    
	
	if (unlikely(free_head == NULL)){
		printk(KERN_ERR "QDMA FQ decriptor not available...\n");
		return 0;
	}
	memset(free_head, 0x0, sizeof(struct QDMA_txdesc) * NUM_QDMA_PAGE);

#if defined (CONFIG_MIPS)
	free_page_head = pci_alloc_consistent(NULL, NUM_QDMA_PAGE * QDMA_PAGE_SIZE, &phy_free_page_head);
#else
	free_page_head = dma_alloc_coherent(dev->dev.parent, NUM_QDMA_PAGE * QDMA_PAGE_SIZE, &phy_free_page_head, GFP_KERNEL);
#endif  


	
	if (unlikely(free_page_head == NULL)){
		printk(KERN_ERR "QDMA FQ page not available...\n");
		return 0;
	}	
	for (i=0; i < NUM_QDMA_PAGE; i++) {
		free_head[i].txd_info1.SDP = (phy_free_page_head + (i * QDMA_PAGE_SIZE));
		if(i < (NUM_QDMA_PAGE-1)){
			free_head[i].txd_info2.NDP = (phy_free_head + ((i+1) * sizeof(struct QDMA_txdesc)));


#if 0
			printk("free_head_phy[%d] is 0x%x!!!\n",i, VIRT_TO_PHYS(&free_head[i]) );
			printk("free_head[%d] is 0x%x!!!\n",i, &free_head[i] );
			printk("free_head[%d].txd_info1.SDP is 0x%x!!!\n",i, free_head[i].txd_info1.SDP );
			printk("free_head[%d].txd_info2.NDP is 0x%x!!!\n",i, free_head[i].txd_info2.NDP );
#endif
		}
		free_head[i].txd_info3.SDL = QDMA_PAGE_SIZE;

	}
	phy_free_tail = (phy_free_head + (u32)((NUM_QDMA_PAGE-1) * sizeof(struct QDMA_txdesc)));

	printk("phy_free_head is 0x%x!!!\n", phy_free_head);
	printk("phy_free_tail_phy is 0x%x!!!\n", phy_free_tail);
	sysRegWrite(QDMA_FQ_HEAD, (u32)phy_free_head);
	sysRegWrite(QDMA_FQ_TAIL, (u32)phy_free_tail);
	sysRegWrite(QDMA_FQ_CNT, ((NUM_TX_DESC << 16) | NUM_QDMA_PAGE));
	sysRegWrite(QDMA_FQ_BLEN, QDMA_PAGE_SIZE << 16);

	ei_local->free_head = free_head;
	ei_local->phy_free_head = phy_free_head;
	ei_local->free_page_head = free_page_head;
	ei_local->phy_free_page_head = phy_free_page_head;
    return 1;
}

int fe_dma_init(struct net_device *dev)
{

	int i;
	unsigned int	regVal;
	END_DEVICE* ei_local = netdev_priv(dev);
	
#if defined (CONFIG_HW_SFQ)
  	sfq_init(dev);
#endif
	fq_qdma_init(dev);
	
	while(1)
	{
		regVal = sysRegRead(QDMA_GLO_CFG);
		if((regVal & RX_DMA_BUSY))
		{
			printk("\n  RX_DMA_BUSY !!! ");
			continue;
		}
		if((regVal & TX_DMA_BUSY))
		{
			printk("\n  TX_DMA_BUSY !!! ");
			continue;
		}
		break;
	}
	/*tx desc alloc, add a NULL TXD to HW*/

	qdma_tx_desc_alloc();

	/* Initial RX Ring 0*/
	
#ifdef CONFIG_32B_DESC
	ei_local->qrx_ring = kmalloc(NUM_QRX_DESC * sizeof(struct PDMA_rxdesc), GFP_KERNEL);
	ei_local->phy_qrx_ring = virt_to_phys(ei_local->qrx_ring);
#else

#if defined (CONFIG_MIPS)
	ei_local->qrx_ring = pci_alloc_consistent(NULL, NUM_QRX_DESC * sizeof(struct PDMA_rxdesc), &ei_local->phy_qrx_ring);
#else
	ei_local->qrx_ring = dma_alloc_coherent(dev->dev.parent, NUM_QRX_DESC * sizeof(struct PDMA_rxdesc), &ei_local->phy_qrx_ring, GFP_KERNEL);
#endif  


	
#endif
	for (i = 0; i < NUM_QRX_DESC; i++) {
		memset(&ei_local->qrx_ring[i],0,sizeof(struct PDMA_rxdesc));
		ei_local->qrx_ring[i].rxd_info2.DDONE_bit = 0;
#if defined (CONFIG_RAETH_SCATTER_GATHER_RX_DMA)
		ei_local->qrx_ring[i].rxd_info2.LS0 = 0;
#ifdef USE_BUILD_SKB
		ei_local->qrx_ring[i].rxd_info2.PLEN0 = SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) +
                   SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
#else
		ei_local->qrx_ring[i].rxd_info2.PLEN0 = MAX_RX_LENGTH;
#endif
#else
		ei_local->qrx_ring[i].rxd_info2.LS0 = 1;
#endif
    
#ifdef USE_BUILD_SKB
		ei_local->qrx_ring[i].rxd_info1.PDP0 = dma_map_single(dev->dev.parent, ei_local->netrx_skbuf[0][i] + NET_SKB_PAD, SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) + SKB_DATA_ALIGN(sizeof(struct skb_shared_info)), DMA_FROM_DEVICE);
#else
		ei_local->qrx_ring[i].rxd_info1.PDP0 = dma_map_single(dev->dev.parent, ei_local->netrx_skbuf[0][i]->data, MAX_RX_LENGTH, DMA_FROM_DEVICE);
#endif
		if (unlikely(dma_mapping_error(dev->dev.parent, ei_local->qrx_ring[i].rxd_info1.PDP0))){
			printk(KERN_ERR "[%s]dma_map_single() failed...\n", __func__);
			goto no_rx_mem;
		}
	}
	printk("\nphy_qrx_ring = 0x%08x, qrx_ring = 0x%p\n",ei_local->phy_qrx_ring,ei_local->qrx_ring);

	regVal = sysRegRead(QDMA_GLO_CFG);
	regVal &= 0x000000FF;

	sysRegWrite(QDMA_GLO_CFG, regVal);
	regVal=sysRegRead(QDMA_GLO_CFG);

	/* Tell the adapter where the TX/RX rings are located. */

	sysRegWrite(QRX_BASE_PTR_0, phys_to_bus((u32) ei_local->phy_qrx_ring));
	sysRegWrite(QRX_MAX_CNT_0,  cpu_to_le32((u32) NUM_QRX_DESC));
	sysRegWrite(QRX_CRX_IDX_0, cpu_to_le32((u32) (NUM_QRX_DESC - 1)));
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
	rx_calc_idx0 = sysRegRead(QRX_CRX_IDX_0);
	ei_local->rx_calc_idx[0] = sysRegRead(QRX_CRX_IDX_0);
#endif
	sysRegWrite(QDMA_RST_CFG, PST_DRX_IDX0);

        ei_local->rx_ring[0] = ei_local->qrx_ring;
#if !defined (CONFIG_RAETH_QDMATX_QDMARX)	
	/* Initial PDMA RX Ring 0*/
#ifdef CONFIG_32B_DESC
        ei_local->rx_ring[0] = kmalloc(NUM_RX_DESC * sizeof(struct PDMA_rxdesc), GFP_KERNEL);
        ei_local->phy_rx_ring0 = virt_to_phys(ei_local->rx_ring[0]);
#else
#if defined (CONFIG_MIPS)
	ei_local->rx_ring[0] = pci_alloc_consistent(NULL, NUM_RX_DESC * sizeof(struct PDMA_rxdesc), &ei_local->phy_rx_ring0);
#else
	ei_local->rx_ring[0] = dma_alloc_coherent(dev->dev.parent, NUM_RX_DESC * sizeof(struct PDMA_rxdesc), &ei_local->phy_rx_ring0, GFP_KERNEL);
#endif  

#endif
	for (i = 0; i < NUM_RX_DESC; i++) {
	memset(&ei_local->rx_ring[0][i],0,sizeof(struct PDMA_rxdesc));
         ei_local->rx_ring[0][i].rxd_info2.DDONE_bit = 0;
#if defined (CONFIG_RAETH_SCATTER_GATHER_RX_DMA)
	ei_local->rx_ring[0][i].rxd_info2.LS0 = 0;
#ifdef USE_BUILD_SKB
		ei_local->rx_ring[0][i].rxd_info2.PLEN0 = SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) +
                   SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
#else
		ei_local->rx_ring[0][i].rxd_info2.PLEN0 = MAX_RX_LENGTH;
#endif
#else
	ei_local->rx_ring[0][i].rxd_info2.LS0 = 1;
#endif
#ifdef USE_BUILD_SKB
		ei_local->rx_ring[0][i].rxd_info1.PDP0 = dma_map_single(dev->dev.parent, ei_local->netrx_skbuf[0][i] + NET_SKB_PAD , SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) + SKB_DATA_ALIGN(sizeof(struct skb_shared_info)), DMA_FROM_DEVICE);
#else
		ei_local->rx_ring[0][i].rxd_info1.PDP0 = dma_map_single(dev->dev.parent, ei_local->netrx_skbuf[0][i]->data, MAX_RX_LENGTH, DMA_FROM_DEVICE);
#endif
	if (unlikely(dma_mapping_error(dev->dev.parent, ei_local->rx_ring[0][i].rxd_info1.PDP0))){
		printk(KERN_ERR "[%s]dma_map_single() failed...\n", __func__);
		goto no_rx_mem;
	}
							        }
        printk("\nphy_rx_ring0 = 0x%08x, rx_ring[0] = 0x%p\n",ei_local->phy_rx_ring0,ei_local->rx_ring[0]);

        regVal = sysRegRead(PDMA_GLO_CFG);
        regVal &= 0x000000FF;
        sysRegWrite(PDMA_GLO_CFG, regVal);
        regVal=sysRegRead(PDMA_GLO_CFG);
        sysRegWrite(RX_BASE_PTR0, phys_to_bus((u32) ei_local->phy_rx_ring0));
        sysRegWrite(RX_MAX_CNT0,  cpu_to_le32((u32) NUM_RX_DESC));
        sysRegWrite(RX_CALC_IDX0, cpu_to_le32((u32) (NUM_RX_DESC - 1)));
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
        rx_calc_idx0 =  sysRegRead(RX_CALC_IDX0);
	ei_local->rx_calc_idx[0] = sysRegRead(RX_CALC_IDX0);
#endif
        sysRegWrite(PDMA_RST_CFG, PST_DRX_IDX0);
#endif	
#if !defined (CONFIG_HW_SFQ)
        /* Enable randon early drop and set drop threshold automatically */
	sysRegWrite(QDMA_FC_THRES, 0x174444);
#endif
	sysRegWrite(QDMA_HRED2, 0x0);
	set_fe_dma_glo_cfg();
#if defined	(CONFIG_ARCH_MT7623) || defined	(CONFIG_ARCH_MT7622)
	printk("Enable QDMA TX NDP coherence check and re-read mechanism\n");
	regVal=sysRegRead(QDMA_GLO_CFG);
	regVal = regVal | 0x400 | 0x100000;
	sysRegWrite(QDMA_GLO_CFG, regVal);
	printk("***********QDMA_GLO_CFG=%x\n", sysRegRead(QDMA_GLO_CFG));
#endif	

	return 0;

no_rx_mem:
	return -ENOMEM;
}

#if defined (CONFIG_HW_SFQ)

int sfq_prot = 0;

#if(sfq_debug)
int udp_source_port=0;
int tcp_source_port=0;
int ack_packt =0;
#endif
int SfqParseLayerInfo(struct sk_buff * skb)
{

	struct vlan_hdr *vh_sfq = NULL;
	struct ethhdr *eth_sfq = NULL;
	struct iphdr *iph_sfq = NULL;
	struct ipv6hdr *ip6h_sfq = NULL;
	struct tcphdr *th_sfq = NULL;
	struct udphdr *uh_sfq = NULL;

	memset(&SfqParseResult, 0, sizeof(SfqParseResult));
	eth_sfq = (struct ethhdr *)skb->data;
	memcpy(SfqParseResult.dmac, eth_sfq->h_dest, ETH_ALEN);
	memcpy(SfqParseResult.smac, eth_sfq->h_source, ETH_ALEN);
	SfqParseResult.eth_type = eth_sfq->h_proto;
	
	if (SfqParseResult.eth_type == htons(ETH_P_8021Q)){
		SfqParseResult.vlan1_gap = VLAN_HLEN;
		vh_sfq = (struct vlan_hdr *)(skb->data + ETH_HLEN);
		SfqParseResult.eth_type = vh_sfq->h_vlan_encapsulated_proto;
	}else{
		SfqParseResult.vlan1_gap = 0;
	}
  
	/* set layer4 start addr */
	if ((SfqParseResult.eth_type == htons(ETH_P_IP)) || (SfqParseResult.eth_type == htons(ETH_P_PPP_SES) 
		&& SfqParseResult.ppp_tag == htons(PPP_IP))) {
			
		iph_sfq = (struct iphdr *)(skb->data + ETH_HLEN + (SfqParseResult.vlan1_gap));

		//prepare layer3/layer4 info
		memcpy(&SfqParseResult.iph, iph_sfq, sizeof(struct iphdr));
		if (iph_sfq->protocol == IPPROTO_TCP) {
			th_sfq = (struct tcphdr *)(skb->data + ETH_HLEN + (SfqParseResult.vlan1_gap) + (iph_sfq->ihl * 4));
			memcpy(&SfqParseResult.th, th_sfq, sizeof(struct tcphdr));
#if(sfq_debug) 
			tcp_source_port = ntohs(SfqParseResult.th.source);
			udp_source_port = 0;
			if(ntohl(SfqParseResult.iph.saddr) == 0xa0a0a04){ // tcp ack packet 
				ack_packt = 1;
			}else { 
				ack_packt = 0;
			}
#endif
			sfq_prot = 2;//IPV4_HNAPT
			if(iph_sfq->frag_off & htons(IP_MF|IP_OFFSET)) {
				return 1;
			}
		} else if (iph_sfq->protocol == IPPROTO_UDP) {
			uh_sfq = (struct udphdr *)(skb->data + ETH_HLEN + (SfqParseResult.vlan1_gap) + iph_sfq->ihl * 4);
			memcpy(&SfqParseResult.uh, uh_sfq, sizeof(struct udphdr));
#if(sfq_debug) 
			udp_source_port = ntohs(SfqParseResult.uh.source);
			tcp_source_port = 0;
			ack_packt = 0;
#endif
			sfq_prot = 2;//IPV4_HNAPT
			if(iph_sfq->frag_off & htons(IP_MF|IP_OFFSET)) {
				return 1;
			}
		}else{
			sfq_prot = 1;
		}
	}else if (SfqParseResult.eth_type == htons(ETH_P_IPV6) || 
			(SfqParseResult.eth_type == htons(ETH_P_PPP_SES) &&
		        SfqParseResult.ppp_tag == htons(PPP_IPV6))) {

			ip6h_sfq = (struct ip6hdr *)(skb->data + ETH_HLEN + (SfqParseResult.vlan1_gap));
			if (ip6h_sfq->nexthdr == NEXTHDR_TCP) {
				sfq_prot = 4;//IPV6_5T
#if(sfq_debug) 	
	     		if(ntohl(SfqParseResult.ip6h.saddr.s6_addr32[3]) == 8){
				ack_packt = 1;
			}else { 
				ack_packt = 0;
			}
#endif
			} else if (ip6h_sfq->nexthdr == NEXTHDR_UDP) {
#if(sfq_debug) 					
				ack_packt = 0;
#endif
				sfq_prot = 4;//IPV6_5T
	
			}else{
				sfq_prot = 3;//IPV6_3T
			}
	}
	return 0;
}
#endif

int rt2880_eth_send(END_DEVICE *ei_local, struct net_device* dev, struct sk_buff *skb, int gmac_no)
{
	unsigned int	length=skb->len;
	struct QDMA_txdesc *cpu_ptr, *prev_cpu_ptr;
	struct QDMA_txdesc dummy_desc;
	struct QDMA_txdesc *free_txd;
	int  ctx_offset;
#ifdef CONFIG_PSEUDO_SUPPORT
	PSEUDO_ADAPTER *pAd;
#endif
	ctx_offset = ei_local->tx_cpu_owner_idx0;
	cpu_ptr = (ei_local->txd_pool + (ctx_offset));
	ei_local->skb_free[ctx_offset] = skb;

	dummy_desc.txd_info1 = cpu_ptr->txd_info1;
	dummy_desc.txd_info2 = cpu_ptr->txd_info2;
	dummy_desc.txd_info3 = cpu_ptr->txd_info3;
	dummy_desc.txd_info4 = cpu_ptr->txd_info4;
	prev_cpu_ptr = cpu_ptr;
	cpu_ptr = &dummy_desc;

	//2. prepare data
	cpu_ptr->txd_info1.SDP = virt_to_phys(skb->data);
#if defined (CONFIG_ARCH_MT7622)
	cpu_ptr->txd_info4.SDL = ((skb->len) >> 14);
	cpu_ptr->txd_info3.SDL = skb->len;
#else
	cpu_ptr->txd_info3.SDL = skb->len;
#endif
#if defined (CONFIG_HW_SFQ)
	SfqParseLayerInfo(skb);
	cpu_ptr->txd_info4.VQID0 = 1;//1:HW hash 0:CPU


#if(0)// for tcp ack use, test use  
	if (ack_packt==1){
		cpu_ptr->txd_info3.QID = 0x0a;
		//cpu_ptr->txd_info3.VQID = 0;
	}else{
		cpu_ptr->txd_info3.QID = 0;
	}
#endif  
	cpu_ptr->txd_info3.PROT = sfq_prot;
	cpu_ptr->txd_info3.IPOFST = 14 + (SfqParseResult.vlan1_gap); //no vlan

#endif
	cpu_ptr->txd_info4.FPORT = gmac_no;

	if (skb->mark < 64)
		cpu_ptr->txd_info3.QID = M2Q_table[skb->mark];
	else{
		printk("skb->mark out of range\n");
		cpu_ptr->txd_info3.QID = 0;
	}

#ifdef CONFIG_PSEUDO_SUPPORT
	if((lan_wan_separate==1) && (gmac_no==2)){
		cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQUp;
		}
#endif			
	}
#if defined (CONFIG_HW_SFQ)
	if((lan_wan_separate==1) && (gmac_no==1)){
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQDl;	
		}
	}
#endif
#endif //end CONFIG_PSEUDO_SUPPORT

	if(dbg==1){
		printk("M2Q_table[%d]=%d\n", skb->mark, M2Q_table[skb->mark]);
		printk("cpu_ptr->txd_info3.QID = %d\n", cpu_ptr->txd_info3.QID);
	}
#if 0 
	iph = (struct iphdr *)skb_network_header(skb);
        if (iph->tos == 0xe0)
		cpu_ptr->txd_info3.QID = 3;
	else if (iph->tos == 0xa0) 
		cpu_ptr->txd_info3.QID = 2;	
        else if (iph->tos == 0x20)
		cpu_ptr->txd_info3.QID = 1;
        else 
		cpu_ptr->txd_info3.QID = 0;
#endif

#if defined (CONFIG_RAETH_CHECKSUM_OFFLOAD) && ! defined(CONFIG_RALINK_RT5350) && !defined (CONFIG_RALINK_MT7628)
	if (skb->ip_summed == CHECKSUM_PARTIAL){
	    cpu_ptr->txd_info4.TUI_CO = 7;
	}else {
	    cpu_ptr->txd_info4.TUI_CO = 0;
	}
#endif

#ifdef CONFIG_RAETH_HW_VLAN_TX
	if(vlan_tx_tag_present(skb)) {
	    cpu_ptr->txd_info4.VLAN_TAG = 0x10000 | vlan_tx_tag_get(skb);
	}else {
	    cpu_ptr->txd_info4.VLAN_TAG = 0;
	}
#endif

#ifdef CONFIG_RAETH_HW_VLAN_TX // QoS Web UI used

	if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==2)){
		cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQUp;
		}
#endif			
	}
#if defined (CONFIG_HW_SFQ)
	if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==1)){
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQDl;	
		}
	}
#endif
#endif // CONFIG_RAETH_HW_VLAN_TX


//no hw van, no GE2, web UI used
#ifndef CONFIG_PSEUDO_SUPPORT
#ifndef CONFIG_RAETH_HW_VLAN_TX 
	if(lan_wan_separate==1){
		struct vlan_hdr *vh = NULL;
		unsigned short vlanid = 0;
    		unsigned short vlan_TCI;
		vh = (struct vlan_hdr *)(skb->data + ETH_HLEN);
		vlan_TCI = vh->h_vlan_TCI;
		vlanid = (vlan_TCI & VLAN_VID_MASK)>>8;
		if(vlanid == 2)//to wan
		{
			cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
			if(web_sfq_enable==1 &&(skb->mark == 2)){ 
				cpu_ptr->txd_info3.QID = HwSfqQUp;
			}
#endif			
		}else if(vlanid == 1){ //to lan
#if defined (CONFIG_HW_SFQ)
			if(web_sfq_enable==1 &&(skb->mark == 2)){ 
				cpu_ptr->txd_info3.QID = HwSfqQDl;	
			}
#endif
		}
	}
#endif
#endif
#if defined (CONFIG_RA_HW_NAT) || defined (CONFIG_RA_HW_NAT_MODULE)
	if(FOE_MAGIC_TAG(skb) == FOE_MAGIC_PPE) {
		if(ra_sw_nat_hook_rx!= NULL){
		    cpu_ptr->txd_info4.FPORT = 4; /* PPE */
		    FOE_MAGIC_TAG(skb) = 0;
	    	}
  	}
#endif
#if 0
	cpu_ptr->txd_info4.FPORT = 4; /* PPE */
	cpu_ptr->txd_info4.UDF = 0x2F;
#endif
	
#if defined (CONFIG_MIPS)	
//	dma_cache_sync(NULL, skb->data, skb->len, DMA_TO_DEVICE);
#else
//	dma_sync_single_for_device(dev->dev.parent, virt_to_phys(skb->data), skb->len, DMA_TO_DEVICE);
#endif
	cpu_ptr->txd_info3.SWC_bit = 1;

	//3. get NULL TXD and decrease free_tx_num by 1.
	ei_local->tx_cpu_owner_idx0 = get_free_txd(ei_local, &free_txd);

	//4. hook new TXD in the end of queue
	//cpu_ptr->txd_info2.NDP = VIRT_TO_PHYS(free_txd);
	cpu_ptr->txd_info2.NDP = (unsigned int)(free_txd);


	//5. move CPU_PTR to new TXD
	//ei_local->tx_cpu_ptr = VIRT_TO_PHYS(free_txd);
	cpu_ptr->txd_info4.TSO = 0;
	cpu_ptr->txd_info3.LS_bit = 1;
	cpu_ptr->txd_info3.OWN_bit = 0;
	prev_cpu_ptr->txd_info1 = dummy_desc.txd_info1;
	prev_cpu_ptr->txd_info2 = dummy_desc.txd_info2;
	prev_cpu_ptr->txd_info3 = dummy_desc.txd_info3;
	prev_cpu_ptr->txd_info4 = dummy_desc.txd_info4;
	//wmb();
	//sysRegWrite(QTX_CTX_PTR, ei_local->tx_cpu_ptr);

#if 0 
	printk("----------------------------------------------\n");
	printk("txd_info1:%08X \n",*(int *)&cpu_ptr->txd_info1);
	printk("txd_info2:%08X \n",*(int *)&cpu_ptr->txd_info2);
	printk("txd_info3:%08X \n",*(int *)&cpu_ptr->txd_info3);
	printk("txd_info4:%08X \n",*(int *)&cpu_ptr->txd_info4);
#endif
	/* NOTE: add memory barrier to avoid DMA access memory earlier than memory written */
	wmb();
	sysRegWrite(QTX_CTX_PTR, (unsigned int)free_txd);

#ifdef CONFIG_PSEUDO_SUPPORT
	if (gmac_no == 2) {
		if (ei_local->PseudoDev != NULL) {
			pAd = netdev_priv(ei_local->PseudoDev);
			pAd->stat.tx_packets++;
			pAd->stat.tx_bytes += length;
		}
	} else

#endif
	{
		ei_local->stat.tx_packets++;
		ei_local->stat.tx_bytes += skb->len;
	}
#ifdef CONFIG_RAETH_NAPI
	if ( ei_local->tx_full == 1) {
		ei_local->tx_full = 0;
		netif_wake_queue(dev);
	}
#endif

	return length;
}

#if defined (CONFIG_RAETH_TSO)
int rt2880_eth_send_tso(END_DEVICE *ei_local, struct net_device* dev, struct sk_buff *skb, int gmac_no)
{
	unsigned int	length=skb->len;
	struct QDMA_txdesc *cpu_ptr, *prev_cpu_ptr;
	struct QDMA_txdesc dummy_desc;
	struct QDMA_txdesc *free_txd;
	int  ctx_offset;
	struct iphdr *iph = NULL;
	struct QDMA_txdesc *init_cpu_ptr;
	struct tcphdr *th = NULL;
	struct skb_frag_struct *frag;
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	unsigned int len, size, offset, frag_txd_num;
	int init_txd_idx, i;


#if defined (CONFIG_RAETH_TSOV6)
	struct ipv6hdr *ip6h = NULL;
#endif

#ifdef CONFIG_PSEUDO_SUPPORT
	PSEUDO_ADAPTER *pAd;
#endif
	ctx_offset = ei_local->tx_cpu_owner_idx0;
	cpu_ptr = (ei_local->txd_pool + (ctx_offset));
	ei_local->skb_free[ctx_offset] = skb;
	init_cpu_ptr = cpu_ptr;
	init_txd_idx = ctx_offset;
	dummy_desc.txd_info1 = cpu_ptr->txd_info1;
	dummy_desc.txd_info2 = cpu_ptr->txd_info2;
	dummy_desc.txd_info3 = cpu_ptr->txd_info3;
	dummy_desc.txd_info4 = cpu_ptr->txd_info4;
	prev_cpu_ptr = cpu_ptr;
	cpu_ptr = &dummy_desc;

	len = length -skb->data_len;
	offset = virt_to_phys(skb->data);
	cpu_ptr->txd_info1.SDP = offset;
	if (len > MAX_TXD_LEN){
		cpu_ptr->txd_info3.SDL = MAX_TXD_LEN;
		cpu_ptr->txd_info3.LS_bit = 0;
		len -= MAX_TXD_LEN;
		offset += MAX_TXD_LEN;
	} else {
		cpu_ptr->txd_info3.SDL = len;
		cpu_ptr->txd_info3.LS_bit = nr_frags ? 0:1;
		len = 0;
	}
#if defined (CONFIG_HW_SFQ)		
	SfqParseLayerInfo(skb);
#if(0)
	cpu_ptr->txd_info4.VQID0 = 0;//1:HW hash 0:CPU
	if (tcp_source_port==1000)  cpu_ptr->txd_info3.VQID = 0;
	else if (tcp_source_port==1100)  cpu_ptr->txd_info3.VQID = 1;
	else if (tcp_source_port==1200)  cpu_ptr->txd_info3.VQID = 2;
	else cpu_ptr->txd_info3.VQID = 0;
#else 
	cpu_ptr->txd_info4.VQID0 = 1;
	cpu_ptr->txd_info3.PROT = sfq_prot;
	cpu_ptr->txd_info3.IPOFST = 14 + (SfqParseResult.vlan1_gap); //no vlan
#endif
#endif
	cpu_ptr->txd_info4.FPORT = gmac_no;

	cpu_ptr->txd_info4.TSO = 0;
	if (skb->mark < 64)
		cpu_ptr->txd_info3.QID = M2Q_table[skb->mark];
	else{
		printk("skb->mark out of range\n");
		cpu_ptr->txd_info3.QID = 0;
	}

#ifdef CONFIG_PSEUDO_SUPPORT //web UI used tso
	if((lan_wan_separate==1) && (gmac_no==2)){
		cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
		if(web_sfq_enable == 1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQUp;	
		}
#endif		
	}
#if defined (CONFIG_HW_SFQ)
	if((lan_wan_separate==1) && (gmac_no==1)){
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQDl;	
		}
	}
#endif
#endif //CONFIG_PSEUDO_SUPPORT
	if(dbg==1){
		printk("M2Q_table[%d]=%d\n", skb->mark, M2Q_table[skb->mark]);
		printk("cpu_ptr->txd_info3.QID = %d\n", cpu_ptr->txd_info3.QID);
	}
#if defined (CONFIG_RAETH_CHECKSUM_OFFLOAD) && ! defined(CONFIG_RALINK_RT5350) && !defined (CONFIG_RALINK_MT7628)
	if (skb->ip_summed == CHECKSUM_PARTIAL){
		cpu_ptr->txd_info4.TUI_CO = 7;
	}else {
		cpu_ptr->txd_info4.TUI_CO = 0;
	}
#endif

#ifdef CONFIG_RAETH_HW_VLAN_TX
	if(vlan_tx_tag_present(skb)) {
		cpu_ptr->txd_info4.VLAN_TAG = 0x10000 | vlan_tx_tag_get(skb);
	}else {
		cpu_ptr->txd_info4.VLAN_TAG = 0;
	}
#endif
#ifdef CONFIG_RAETH_HW_VLAN_TX // QoS Web UI used tso

	if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==2)){
		//cpu_ptr->txd_info3.QID += 8;
		cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQUp;
		}
#endif			
	}
#if defined (CONFIG_HW_SFQ)
	if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==1)){
		if(web_sfq_enable==1 &&(skb->mark == 2)){ 
			cpu_ptr->txd_info3.QID = HwSfqQDl;	
		}
	}
#endif
#endif // CONFIG_RAETH_HW_VLAN_TX


	//no hw van, no GE2, web UI used
#ifndef CONFIG_PSEUDO_SUPPORT
#ifndef CONFIG_RAETH_HW_VLAN_TX 
	if(lan_wan_separate==1){
		struct vlan_hdr *vh = NULL;
		unsigned short vlanid = 0;
		unsigned short vlan_TCI;
		vh = (struct vlan_hdr *)(skb->data + ETH_HLEN);
		vlan_TCI = vh->h_vlan_TCI;
		vlanid = (vlan_TCI & VLAN_VID_MASK)>>8;
		if(vlanid == 2)//eth2.2 to wan
		{
			cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
			if(web_sfq_enable==1 &&(skb->mark == 2)){ 
				cpu_ptr->txd_info3.QID = HwSfqQUp;
			}
#endif			
		}else if(!strcmp(netdev, "eth2.1")){ // eth2.1 to lan
#if defined (CONFIG_HW_SFQ)
			if(web_sfq_enable==1 &&(skb->mark == 2)){ 
				cpu_ptr->txd_info3.QID = HwSfqQDl;	
			}
#endif
		}
	}
#endif
#endif

#if defined (CONFIG_RA_HW_NAT) || defined (CONFIG_RA_HW_NAT_MODULE)
	if(FOE_MAGIC_TAG(skb) == FOE_MAGIC_PPE) {
		if(ra_sw_nat_hook_rx!= NULL){
			cpu_ptr->txd_info4.FPORT = 4; /* PPE */
			FOE_MAGIC_TAG(skb) = 0;
		}
	}
#endif

	cpu_ptr->txd_info3.SWC_bit = 1;

	ctx_offset = get_free_txd(ei_local, &free_txd);
	//cpu_ptr->txd_info2.NDP = VIRT_TO_PHYS(free_txd);
	//ei_local->tx_cpu_ptr = VIRT_TO_PHYS(free_txd);
	cpu_ptr->txd_info2.NDP = (unsigned int)free_txd;
	prev_cpu_ptr->txd_info1 = dummy_desc.txd_info1;
	prev_cpu_ptr->txd_info2 = dummy_desc.txd_info2;
	prev_cpu_ptr->txd_info3 = dummy_desc.txd_info3;
	prev_cpu_ptr->txd_info4 = dummy_desc.txd_info4;
	if (len > 0) {
		offset = virt_to_phys(skb->data) + MAX_TXD_LEN;
		frag_txd_num = cal_frag_txd_num(len);
		for(frag_txd_num = frag_txd_num;frag_txd_num > 0; frag_txd_num --){
			if(len < MAX_TXD_LEN)
				size = len;
			else
				size = MAX_TXD_LEN;

			cpu_ptr = (ei_local->txd_pool + (ctx_offset));
			dummy_desc.txd_info1 = cpu_ptr->txd_info1;
			dummy_desc.txd_info2 = cpu_ptr->txd_info2;
			dummy_desc.txd_info3 = cpu_ptr->txd_info3;
			dummy_desc.txd_info4 = cpu_ptr->txd_info4;
			prev_cpu_ptr = cpu_ptr;
			cpu_ptr = &dummy_desc;
			cpu_ptr->txd_info3.QID = M2Q_table[skb->mark];
			cpu_ptr->txd_info1.SDP = offset;
			cpu_ptr->txd_info3.SDL = size;
			if((nr_frags == 0) && (frag_txd_num == 1))
				cpu_ptr->txd_info3.LS_bit = 1;
			else
				cpu_ptr->txd_info3.LS_bit = 0;
			cpu_ptr->txd_info3.OWN_bit = 0;
			cpu_ptr->txd_info3.SWC_bit = 1;
			ei_local->skb_free[ctx_offset] = (cpu_ptr->txd_info3.LS_bit == 1)?skb:(struct  sk_buff *)0xFFFFFFFF; //MAGIC ID
			ctx_offset = get_free_txd(ei_local, &free_txd);
			cpu_ptr->txd_info2.NDP = (unsigned int)free_txd;
			prev_cpu_ptr->txd_info1 = dummy_desc.txd_info1;
			prev_cpu_ptr->txd_info2 = dummy_desc.txd_info2;
			prev_cpu_ptr->txd_info3 = dummy_desc.txd_info3;
			prev_cpu_ptr->txd_info4 = dummy_desc.txd_info4;
			offset += size;
			len -= size;
		}
		if(nr_frags == 0)
			ei_local->skb_free[init_txd_idx]= (struct  sk_buff *)0xFFFFFFFF; //MAGIC ID

	}

	if(nr_frags > 0) {
		for(i=0;i<nr_frags;i++) {
			// 1. set or get init value for current fragment
			offset = 0;  
			frag = &skb_shinfo(skb)->frags[i];
			len = frag->size; 
			frag_txd_num = cal_frag_txd_num(len); // calculate the needed TXD numbers for this fragment
			for(frag_txd_num = frag_txd_num;frag_txd_num > 0; frag_txd_num --){
				// 2. size will be assigned to SDL and can't be larger than MAX_TXD_LEN
				if(len < MAX_TXD_LEN)
					size = len;
				else
					size = MAX_TXD_LEN;			

				//3. Update TXD info
				cpu_ptr = (ei_local->txd_pool + (ctx_offset));
				dummy_desc.txd_info1 = cpu_ptr->txd_info1;
				dummy_desc.txd_info2 = cpu_ptr->txd_info2;
				dummy_desc.txd_info3 = cpu_ptr->txd_info3;
				dummy_desc.txd_info4 = cpu_ptr->txd_info4;
				prev_cpu_ptr = cpu_ptr;
				cpu_ptr = &dummy_desc;
				if (skb->mark < 64)
					cpu_ptr->txd_info3.QID = M2Q_table[skb->mark];
				else{
					printk("skb->mark out of range\n");
					cpu_ptr->txd_info3.QID = 0;
				}

#ifdef CONFIG_PSEUDO_SUPPORT //QoS Web UI used , nr_frags
				if((lan_wan_separate==1) && (gmac_no==2)){
					//cpu_ptr->txd_info3.QID += 8;
					cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
					if(web_sfq_enable==1 &&(skb->mark == 2)){ 
						cpu_ptr->txd_info3.QID = HwSfqQUp;	
					}
#endif
				}
#if defined (CONFIG_HW_SFQ)				
				if((lan_wan_separate==1) && (gmac_no==1)){
					if(web_sfq_enable==1 &&(skb->mark == 2)){ 
						cpu_ptr->txd_info3.QID = HwSfqQDl;	
					}
				}
#endif
#endif //CONFIG_PSEUDO_SUPPORT

				//QoS web used, nr_frags
#ifdef CONFIG_RAETH_HW_VLAN_TX 
				if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==2)){
					cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
					if(web_sfq_enable==1 &&(skb->mark == 2)){ 
						cpu_ptr->txd_info3.QID = HwSfqQUp;
					}
#endif			
				}
#if defined (CONFIG_HW_SFQ)
				if((lan_wan_separate==1) && (vlan_tx_tag_get(skb)==1)){
					if(web_sfq_enable==1 &&(skb->mark == 2)){ 
						cpu_ptr->txd_info3.QID = HwSfqQDl;	
					}
				}
#endif
#endif // CONFIG_RAETH_HW_VLAN_TX
				//no hw van, no GE2, web UI used
#ifndef CONFIG_PSEUDO_SUPPORT
#ifndef CONFIG_RAETH_HW_VLAN_TX 
				if(lan_wan_separate==1){
					struct vlan_hdr *vh = NULL;
					unsigned short vlanid = 0;
					unsigned short vlan_TCI;
					vh = (struct vlan_hdr *)(skb->data + ETH_HLEN);
					vlan_TCI = vh->h_vlan_TCI;
					vlanid = (vlan_TCI & VLAN_VID_MASK)>>8;
					if(vlanid == 2)//eth2.2 to wan
					{
						cpu_ptr->txd_info3.QID += 8;
#if defined (CONFIG_HW_SFQ)
						if(web_sfq_enable==1 &&(skb->mark == 2)){ 
							cpu_ptr->txd_info3.QID = HwSfqQUp;
						}
#endif			
					}else if(vlanid == 1){ // eth2.1 to lan
#if defined (CONFIG_HW_SFQ)	
						if(web_sfq_enable==1 &&(skb->mark == 2)){ 
							cpu_ptr->txd_info3.QID = HwSfqQDl;	
						}
#endif
					}
				}
#endif
#endif
				if(dbg==1){
					printk("M2Q_table[%d]=%d\n", skb->mark, M2Q_table[skb->mark]);
					printk("cpu_ptr->txd_info3.QID = %d\n", cpu_ptr->txd_info3.QID);
				}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
				cpu_ptr->txd_info1.SDP = pci_map_page(NULL, frag->page, frag->page_offset, frag->size, PCI_DMA_TODEVICE);
#else
#if defined (CONFIG_MIPS)//harry mt7622
				cpu_ptr->txd_info1.SDP = pci_map_page(NULL, frag->page.p, frag->page_offset + offset, size, PCI_DMA_TODEVICE);
				//				printk(" frag->page = %08x. frag->page_offset = %08x. frag->size = % 08x.\n", frag->page, (frag->page_offset+offset), size);
#else
				cpu_ptr->txd_info1.SDP  = dma_map_page(dev->dev.parent, frag->page.p, frag->page_offset + offset, size, DMA_TO_DEVICE);
#endif
#endif
				cpu_ptr->txd_info3.SDL = size;
#if defined (CONFIG_QDMA_NEW_FEATURE)
				cpu_ptr->txd_info4.SDL = (size >> 14);
#endif
				if( (i==(nr_frags-1)) && (frag_txd_num == 1))
					cpu_ptr->txd_info3.LS_bit = 1;
				else
					cpu_ptr->txd_info3.LS_bit = 0;
				cpu_ptr->txd_info3.OWN_bit = 0;
				cpu_ptr->txd_info3.SWC_bit = 1;
				//4. Update skb_free for housekeeping
				ei_local->skb_free[ctx_offset] = (cpu_ptr->txd_info3.LS_bit == 1)?skb:(struct  sk_buff *)0xFFFFFFFF; //MAGIC ID

				//5. Get next TXD
				ctx_offset = get_free_txd(ei_local, &free_txd);
				//cpu_ptr->txd_info2.NDP = VIRT_TO_PHYS(free_txd);
				//ei_local->tx_cpu_ptr = VIRT_TO_PHYS(free_txd);
				cpu_ptr->txd_info2.NDP = (unsigned int)free_txd;
				prev_cpu_ptr->txd_info1 = dummy_desc.txd_info1;
				prev_cpu_ptr->txd_info2 = dummy_desc.txd_info2;
				prev_cpu_ptr->txd_info3 = dummy_desc.txd_info3;
				prev_cpu_ptr->txd_info4 = dummy_desc.txd_info4;
				//6. Update offset and len.
				offset += size;
				len -= size;
			}
		}
		ei_local->skb_free[init_txd_idx]= (struct  sk_buff *)0xFFFFFFFF; //MAGIC ID
	}
	ei_local->tx_cpu_owner_idx0 = ctx_offset;

	if(skb_shinfo(skb)->gso_segs > 1) {

		//		TsoLenUpdate(skb->len);

		/* TCP over IPv4 */
		iph = (struct iphdr *)skb_network_header(skb);
#if defined (CONFIG_RAETH_TSOV6)
		/* TCP over IPv6 */
		ip6h = (struct ipv6hdr *)skb_network_header(skb);
#endif				
		if((iph->version == 4) && (iph->protocol == IPPROTO_TCP)) {
			th = (struct tcphdr *)skb_transport_header(skb);

			init_cpu_ptr->txd_info4.TSO = 1;

			th->check = htons(skb_shinfo(skb)->gso_size);
#if defined (CONFIG_MIPS)	
			dma_cache_sync(NULL, th, sizeof(struct tcphdr), DMA_TO_DEVICE);
#else
			dma_sync_single_for_device(dev->dev.parent, virt_to_phys(th), sizeof(struct tcphdr), DMA_TO_DEVICE);
#endif
		} 

#if defined (CONFIG_RAETH_TSOV6)
		/* TCP over IPv6 */
		//ip6h = (struct ipv6hdr *)skb_network_header(skb);
		else if ((ip6h->version == 6) && (ip6h->nexthdr == NEXTHDR_TCP)) {
			th = (struct tcphdr *)skb_transport_header(skb);
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
			init_cpu_ptr->txd_info4.TSO = 1;
#else
			init_cpu_ptr->txd_info4.TSO = 1;
#endif
			th->check = htons(skb_shinfo(skb)->gso_size);
#if defined (CONFIG_MIPS)	
			dma_cache_sync(NULL, th, sizeof(struct tcphdr), DMA_TO_DEVICE);
#else
			dma_sync_single_for_device(dev->dev.parent, virt_to_phys(th), sizeof(struct tcphdr), DMA_TO_DEVICE);
#endif
		}
#endif
#if defined (CONFIG_HW_SFQ)
#if(0)
		init_cpu_ptr->txd_info4.VQID0 = 0;//1:HW hash 0:CPU
		if (tcp_source_port==1000)  init_cpu_ptr->txd_info3.VQID = 0;
		else if (tcp_source_port==1100)  init_cpu_ptr->txd_info3.VQID = 1;
		else if (tcp_source_port==1200)  init_cpu_ptr->txd_info3.VQID = 2;
		else cpu_ptr->txd_info3.VQID = 0;
#else 
		init_cpu_ptr->txd_info4.VQID0 = 1;
		init_cpu_ptr->txd_info3.PROT = sfq_prot;
		init_cpu_ptr->txd_info3.IPOFST = 14 + (SfqParseResult.vlan1_gap); //no vlan
#endif
#endif
	}


	//	dma_cache_sync(NULL, skb->data, skb->len, DMA_TO_DEVICE);  

	init_cpu_ptr->txd_info3.OWN_bit = 0;

	/* NOTE: add memory barrier to avoid DMA access memory earlier than memory written */
	wmb();

	sysRegWrite(QTX_CTX_PTR, (unsigned int)free_txd);

#ifdef CONFIG_PSEUDO_SUPPORT
	if (gmac_no == 2) {
		if (ei_local->PseudoDev != NULL) {
			pAd = netdev_priv(ei_local->PseudoDev);
			pAd->stat.tx_packets++;
			pAd->stat.tx_bytes += length;
		}
	} else

#endif
	{
		ei_local->stat.tx_packets++;
		ei_local->stat.tx_bytes += skb->len;
	}
#ifdef CONFIG_RAETH_NAPI
	if ( ei_local->tx_full == 1) {
		ei_local->tx_full = 0;
		netif_wake_queue(dev);
	}
#endif

	return length;
}
#endif

int ei_start_xmit(struct sk_buff* skb, struct net_device *dev, int gmac_no)
{
	END_DEVICE *ei_local = netdev_priv(dev);
	unsigned long flags;
	unsigned int num_of_txd = 0;
#if defined (CONFIG_RAETH_TSO)
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags, i;
	struct skb_frag_struct *frag;
#endif
#ifdef CONFIG_PSEUDO_SUPPORT
	PSEUDO_ADAPTER *pAd;
#endif

#if !defined(CONFIG_RA_NAT_NONE)
	if(ra_sw_nat_hook_tx!= NULL)
	{
		//	   spin_lock_irqsave(&ei_local->page_lock, flags);
		if(ra_sw_nat_hook_tx(skb, gmac_no)==1){
			//	   	spin_unlock_irqrestore(&ei_local->page_lock, flags);
		}else{
			//kfree_skb(skb);
			dev_kfree_skb_any(skb);
			//	   	spin_unlock_irqrestore(&ei_local->page_lock, flags);
			return 0;
		}
	}
#endif



	dev->trans_start = jiffies;	/* save the timestamp */
	spin_lock_irqsave(&ei_local->page_lock, flags);	
#if defined (CONFIG_MIPS)	
	dma_cache_sync(NULL, skb->data, skb_headlen(skb), DMA_TO_DEVICE);
#else
	dma_sync_single_for_device(dev->dev.parent, virt_to_phys(skb->data), skb->len, DMA_TO_DEVICE);
#endif


	//check free_txd_num before calling rt288_eth_send()

#if defined (CONFIG_RAETH_TSO)
	num_of_txd += cal_frag_txd_num(skb->len - skb->data_len);
	if(nr_frags != 0){
		for(i=0;i<nr_frags;i++) {
			frag = &skb_shinfo(skb)->frags[i];
			num_of_txd  += cal_frag_txd_num(frag->size);
		}
	}
#else
	num_of_txd = 1;
#endif

#if defined(CONFIG_RALINK_MT7621)
	if((sysRegRead(0xbe00000c)&0xFFFF) == 0x0101) {
		ei_xmit_housekeeping(0);
	}
#endif
	
    if (atomic_read(&ei_local->free_txd_num) > (num_of_txd + 1))
    {
#if defined (CONFIG_RAETH_TSO)
	    if (num_of_txd > 1)
		    rt2880_eth_send_tso(ei_local, dev, skb, gmac_no);
	    else
		    rt2880_eth_send(ei_local, dev, skb, gmac_no);
#else
		    rt2880_eth_send(ei_local, dev, skb, gmac_no);
#endif
		if (atomic_read(&ei_local->free_txd_num) < 3)
		{
#if defined (CONFIG_RAETH_SW_FC) 		    
		    netif_stop_queue(dev);
#ifdef CONFIG_PSEUDO_SUPPORT
		    netif_stop_queue(ei_local->PseudoDev);
#endif
		    tx_ring_full = 1;
#endif
		}
    } else {  
#ifdef CONFIG_PSEUDO_SUPPORT
		if (gmac_no == 2) 
		{
			if (ei_local->PseudoDev != NULL) 
			{
			    pAd = netdev_priv(ei_local->PseudoDev);
			    pAd->stat.tx_dropped++;
		    }
		} else
#endif
		ei_local->stat.tx_dropped++;
#if defined (CONFIG_RAETH_SW_FC)
                printk("tx_ring_full, drop packet\n");
#endif		
		//kfree_skb(skb);
		dev_kfree_skb_any(skb);
                spin_unlock_irqrestore(&ei_local->page_lock, flags);
		return 0;
     }	
	spin_unlock_irqrestore(&ei_local->page_lock, flags);
	return 0;
}

void ei_xmit_housekeeping(unsigned long unused)
{
    struct net_device *dev = dev_raether;
    END_DEVICE *ei_local = netdev_priv(dev);
#ifndef CONFIG_RAETH_NAPI
    unsigned long reg_int_mask=0;
#endif
    struct QDMA_txdesc *dma_ptr = NULL;
    struct QDMA_txdesc *cpu_ptr = NULL;
    struct QDMA_txdesc *tmp_ptr = NULL;
    unsigned int ctx_offset = 0;
    unsigned int dtx_offset = 0;
    unsigned int release_count = 0;

    dma_ptr = (struct QDMA_txdesc *)sysRegRead(QTX_DRX_PTR);
    ctx_offset = ei_local->tx_cpu_release_idx0;
    dtx_offset = GET_TXD_OFFSET(ei_local, &dma_ptr);
    cpu_ptr = (ei_local->txd_pool + (ctx_offset));

    while(ctx_offset != dtx_offset && (cpu_ptr->txd_info3.OWN_bit == 1)) {
	    //1. keep cpu next TXD
	    tmp_ptr = (struct QDMA_txdesc *)cpu_ptr->txd_info2.NDP;
	    //2. release TXD
	    put_free_txd(ei_local, ctx_offset);
	    //3. update ctx_offset and free skb memory
	    ctx_offset = GET_TXD_OFFSET(ei_local, &tmp_ptr);
#if defined (CONFIG_RAETH_TSO)
	    if(ei_local->skb_free[ctx_offset]!=(struct  sk_buff *)0xFFFFFFFF) {
		    dev_kfree_skb_any(ei_local->skb_free[ctx_offset]);
	    }
#else
	    dev_kfree_skb_any(ei_local->skb_free[ctx_offset]);
#endif
	    ei_local->skb_free[ctx_offset] = 0;
	    //4. update cpu_ptr
	    cpu_ptr = (ei_local->txd_pool + ctx_offset);
	    release_count++;
    }
    if (release_count > 0) {
	    atomic_add(release_count, &ei_local->free_txd_num);
	    netif_wake_queue(dev);
#ifdef CONFIG_PSEUDO_SUPPORT
	    netif_wake_queue(ei_local->PseudoDev);
#endif
	    tx_ring_full=0;
	    ei_local->tx_cpu_release_idx0 = ctx_offset;
    }
	sysRegWrite(QTX_CRX_PTR, (ei_local->phy_txd_pool + (ctx_offset << 4)));
#ifndef CONFIG_RAETH_NAPI
    reg_int_mask=sysRegRead(QFE_INT_ENABLE);
#if defined (DELAY_INT)
    sysRegWrite(QFE_INT_ENABLE, reg_int_mask| RLS_DLY_INT);
#else

    sysRegWrite(QFE_INT_ENABLE, reg_int_mask | RLS_DONE_INT);
#endif
#endif //CONFIG_RAETH_NAPI//
}

EXPORT_SYMBOL(ei_start_xmit);
EXPORT_SYMBOL(ei_xmit_housekeeping);
EXPORT_SYMBOL(fe_dma_init);
EXPORT_SYMBOL(rt2880_eth_send);
