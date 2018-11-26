- Set MANROFFOPT=-mja for Japanese line wrap if using with man command.

% env MANROFFOPT=-mja man drbd.conf

- When updated, use msgmerge.

% msgmerge -N --no-location -U ja/v9/drbd.xml.po v9/drbd.xml.pot

  Then add new translation at the line of fuzzy key or empty msgstr.
