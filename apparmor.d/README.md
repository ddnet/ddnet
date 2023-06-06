# apparmor profiles

Move the profiles into ``/etc/apparmor.d``

Verify they show up in ``aa-status``

The profiles do have the flag complain set. Meaning nothing is actually enforced.
If you want to apply actual security hardening remove the ``flags=(complain)`` from the
beginning of the profile file

After updating the profile file run ``systemctl reload apparmor.service``

If the ddnet server stops working check ``dmesg`` for errors.

