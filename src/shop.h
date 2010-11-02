#ifndef __SHOP_H
#define __SHOP_H

//*****************************************************************************
//
// shop.h
//
// Shops allow mobs or containers to act as stores for selling and buying
// other objects.
//
//*****************************************************************************



SHOP_DATA *newShop   (shop_vnum vnum);
void       deleteShop(SHOP_DATA *shop);

void       shopCopyTo(SHOP_DATA *from, SHOP_DATA *to);
SHOP_DATA *shopCopy  (SHOP_DATA *shop);



#endif // __SHOP_H
