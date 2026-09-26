-- Transmogrification item appearances
CREATE TABLE IF NOT EXISTS `item_transmogrification` (
  `item` int unsigned NOT NULL COMMENT 'Item GUID',
  `transEntry` int unsigned NOT NULL COMMENT 'Display item entry',
  PRIMARY KEY (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
