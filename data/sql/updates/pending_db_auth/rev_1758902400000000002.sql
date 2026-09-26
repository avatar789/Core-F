-- Premium, VIP and custom store tables
CREATE TABLE IF NOT EXISTS `account_premium` (
  `id` int unsigned NOT NULL COMMENT 'Account ID',
  `active` tinyint unsigned NOT NULL DEFAULT '0',
  `setdate` int unsigned NOT NULL DEFAULT '0',
  `unsetdate` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_vip` (
  `id` int unsigned NOT NULL COMMENT 'Account ID',
  `active` tinyint unsigned NOT NULL DEFAULT '0',
  `setdate` int unsigned NOT NULL DEFAULT '0',
  `unsetdate` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_donate` (
  `id` int unsigned NOT NULL COMMENT 'Account ID',
  `bonuses` int NOT NULL DEFAULT '0',
  `votes` int NOT NULL DEFAULT '0',
  `totalBonuses` int NOT NULL DEFAULT '0',
  `totalVotes` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `custom_store_logs` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `character_ID` int unsigned NOT NULL DEFAULT '0',
  `character_name` varchar(12) NOT NULL DEFAULT '',
  `account_ID` int unsigned NOT NULL DEFAULT '0',
  `serviceName` varchar(64) NOT NULL DEFAULT '',
  `itemID` int unsigned NOT NULL DEFAULT '0',
  `itemCount` int unsigned NOT NULL DEFAULT '0',
  `totalPrice` int unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `custom_store_shop_version` (
  `version` int unsigned NOT NULL DEFAULT '1'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `custom_store_shop_version` WHERE `version` = 1;
INSERT INTO `custom_store_shop_version` (`version`) VALUES (1);

CREATE TABLE IF NOT EXISTS `custom_store_item_data` (
  `id` int unsigned NOT NULL,
  `itemEntry` int unsigned NOT NULL DEFAULT '0',
  `count` int unsigned NOT NULL DEFAULT '1',
  `price` int unsigned NOT NULL DEFAULT '0',
  `discount` tinyint unsigned NOT NULL DEFAULT '0',
  `discountPrice` int unsigned NOT NULL DEFAULT '0',
  `creatureEntry` int unsigned NOT NULL DEFAULT '0',
  `storeFlags` int unsigned NOT NULL DEFAULT '0',
  `CategoryID` tinyint unsigned NOT NULL DEFAULT '0',
  `SubCategoryID` tinyint unsigned NOT NULL DEFAULT '0',
  `MoneyID` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `custom_store_special_offer` (
  `offerID` int unsigned NOT NULL,
  `background` varchar(128) NOT NULL DEFAULT '',
  `headline` varchar(128) NOT NULL DEFAULT '',
  `title` varchar(128) NOT NULL DEFAULT '',
  `description` varchar(512) NOT NULL DEFAULT '',
  `detailsTitle` varchar(128) NOT NULL DEFAULT '',
  `details` int unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  `productID` int unsigned NOT NULL DEFAULT '0',
  `itemEntry` int unsigned NOT NULL DEFAULT '0',
  `price` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`offerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `custom_store_special_offer_details` (
  `detailsID` int unsigned NOT NULL DEFAULT '0',
  `itemID` int unsigned NOT NULL DEFAULT '0',
  `role` int unsigned NOT NULL DEFAULT '0',
  `count` int unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `custom_store_mounts` (
  `id` int unsigned NOT NULL,
  `hash` varchar(64) NOT NULL DEFAULT '',
  `currency` tinyint unsigned NOT NULL DEFAULT '1',
  `price` int unsigned NOT NULL DEFAULT '0',
  `productID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
