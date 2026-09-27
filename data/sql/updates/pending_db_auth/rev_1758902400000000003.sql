-- Premium status: one day per purchased unit, paid with bonuses
DELETE FROM `custom_store_item_data` WHERE `productID` = 1000019;
INSERT INTO `custom_store_item_data`
(`productID`, `itemEntry`, `count`, `price`, `discount`, `discountPrice`, `creatureEntry`, `storeFlags`, `CategoryID`, `SubCategoryID`, `MoneyID`)
VALUES
(1000019, 1000019, 1, 700, 0, 0, 0, 0, 6, 0, 1);

UPDATE `custom_store_shop_version` SET `version` = `version` + 1;
