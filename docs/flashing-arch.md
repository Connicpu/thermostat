# Thermostat V3: first-time flashing on Arch Linux

This guide takes you from a fresh Arch install to a flashed, paired Thermostat V3 (ESP32-C6, ESP-IDF v5.3.2, esp-zigbee-lib 1.6.8 and esp-zboss-lib 1.6.4). It follows the repo's `firmware/README.md`, the CI workflow (`.github/workflows/firmware.yml`) and Espressif's v5.3.2 Linux setup page.

> **Prototype firmware.** The firmware README says it has **not run on real hardware yet**. Treat the first flash as bring-up and go through the *Bring-up checklist* (step 7) before you wire it to the HVAC system.

> ⚠️ **Never connect USB while 24 VAC is wired.** Board ground is the bridge rectifier's negative node, not C. With 24 VAC connected, a USB cable to a grounded PC can push current through a rectifier diode and the PC's ground, which can damage the board, the PC or the transformer. Flash on the bench with the board **powered from USB only**, with the HVAC terminal block unplugged and the board out of its case. The case has no USB opening on purpose. (The root README allows an exception only if you use a USB isolator.)

---

## 1. Install the host packages

These are the Arch packages Espressif lists for ESP-IDF v5.3.2, plus `python-pip`:

```sh
sudo pacman -S --needed gcc git make flex bison gperf python python-pip cmake ninja ccache dfu-util libusb
```

`ccache` is optional, but it speeds up rebuilds. You don't need anything from the AUR. ESP-IDF installs its own RISC-V toolchain, esptool and a Python virtualenv under `~/.espressif`.

## 2. Install ESP-IDF v5.3.2

CI builds with exactly v5.3.2 (`espressif/idf:v5.3.2` in `.github/workflows/firmware.yml`; `firmware/dependencies.lock` also pins IDF 5.3.2), so use that tag:

```sh
mkdir -p ~/esp
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32c6
```

Then, in every new shell where you want to build:

```sh
. ~/esp/esp-idf/export.sh
```

(You can add an alias such as `alias get_idf='. ~/esp/esp-idf/export.sh'` to `~/.bashrc`.)

### The Arch Python caveat

Espressif's v5.3.2 docs only say "Python 3.8 or newer". Arch tracks the newest CPython, which may be newer than anything v5.3.2 was tested against. If `install.sh` fails while it creates the Python environment (pip resolution or build errors), try these fixes in order:

1. **Clear stale state and retry.** A pip-installed `virtualenv` in `~/.local` can clash with Arch's packaged one ([esp-idf#7809](https://github.com/espressif/esp-idf/issues/7809)):
   ```sh
   rm -rf ~/.espressif/python_env
   pip uninstall --user virtualenv 2>/dev/null   # only if you installed it with pip
   ~/esp/esp-idf/install.sh esp32c6
   ```
2. **Use an older Python for IDF only.** Install Python 3.12 with pyenv (`sudo pacman -S pyenv`) or with uv. Then put it first on `PATH` when you run `install.sh` and `export.sh`. IDF picks up whichever `python3` it finds first and names the venv after that version.
3. **Build in the same container CI uses.** This route doesn't depend on Arch's Python at all (`sudo pacman -S docker`, then start the docker service). See the box at the end of step 4.

## 3. Serial port permissions

On Arch, serial devices belong to the **`uucp`** group (not `dialout`, which is the Debian/Ubuntu name):

```sh
sudo usermod -aG uucp "$USER"
```

**Log out and back in** (or reboot) for the change to take effect. `groups` should then list `uucp`.

The C6's native USB-Serial-JTAG has USB ID `303a:1001` and shows up as `/dev/ttyACM0`. Group membership is enough to flash over it. A udev rule is only needed if you also want **JTAG debugging** through OpenOCD. In that case, install the rules file that ships with Espressif's OpenOCD (after `install.sh`, look for `share/openocd/contrib/60-openocd.rules` under `~/.espressif/tools/openocd-esp32/*/openocd-esp32/`) or write a minimal one:

```sh
echo 'ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0660", GROUP="uucp", TAG+="uaccess"' \
  | sudo tee /etc/udev/rules.d/60-esp32-usb-jtag.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## 4. Get the code and build it the way CI does

```sh
git clone https://github.com/Connicpu/thermostat.git
cd thermostat/firmware
. ~/esp/esp-idf/export.sh
```

**Production build.** This is the same command as CI's *Production build + release guard* job. CI fails on any compiler warning, so the grep reproduces that check:

```sh
idf.py set-target esp32c6 build 2>&1 | tee build.log
grep -E '(^|[^-])warning:' build.log && echo "WARNINGS (CI would fail)" || echo "clean build"
```

The first build downloads `espressif/esp-zigbee-lib` 1.6.8 and `espressif/esp-zboss-lib` 1.6.4 through the component manager. `main/idf_component.yml` asks for `~1.6.0`; `dependencies.lock` pins those two versions, so you need network access the first time.

**Release guard.** This checks that no simulator or test code ended up in the ELF:

```sh
tools/check_release.sh
# -> release check OK: build/thermostat_v3.elf has no simulator code
```

There are no special environment variables or extra sdkconfig profiles for production. The build uses `sdkconfig.defaults` only. Some things it sets that are worth knowing:

- Target esp32c6, **8 MB flash** (ESP32-C6-WROOM-1-N8), custom `partitions.csv`
- Zigbee **end device** (ZED), native radio
- Console on **USB-Serial-JTAG** (GPIO12/13; UART0 isn't connected)
- App rollback enabled, task watchdog 5 s, power management/light sleep on
- Kconfig defaults (`idf.py menuconfig` → *Thermostat V3* / *Thermostat Zigbee*): °F display, STWD100 external watchdog armed, OTA manufacturer **0x434E**, image type 0x0301, HW version 3

`sdkconfig.variant` is a CI-only check. Applied on top of `sdkconfig.defaults`, it turns **off** `CONFIG_THERMO_DISPLAY_FAHRENHEIT`, `CONFIG_THERMO_HW_WDT_ENABLE` and `CONFIG_PM_ENABLE`. CI also passes `-DEPD_ROTATION=3` on that build. **Don't use it for a real image.** The one variant knob you might actually need is `-DEPD_ROTATION=3`, if the screen comes out mirrored or upside down (see step 7).

> **Container alternative (no host Python issues):** from `thermostat/firmware`:
> ```sh
> docker run --rm -it -v "$PWD":/project -w /project espressif/idf:v5.3.2 \
>   bash -c '. "$IDF_PATH/export.sh" && idf.py set-target esp32c6 build && tools/check_release.sh'
> ```
> To flash from the container, add `--device=/dev/ttyACM0`. Or flash from the host with esptool using the command `idf.py build` prints at the end (the files are in `build/`).

### What lands in flash

| Partition | Purpose |
|---|---|
| `nvs` (0x9000, 24 KB) | Settings, boot-loop counter |
| `otadata`, `phy_init` | OTA slot selection, RF calibration |
| `zb_storage` (64 KB) | Zigbee network state (join info) |
| `zb_fct` (4 KB) | Zigbee factory data |
| `ota_0` / `ota_1` (2 MB each) | Two app slots for Zigbee OTA |

There's no separate `factory` app partition: USB flashing writes the app to `ota_0`. The OTA manufacturer code 0x434E comes from Kconfig (`THERMO_OTA_MANUFACTURER`). The 0xFC57 boost cluster uses the same code as a compile-time constant in `components/zigbee/include/zb_boost.h`, not from that Kconfig option. Nothing per-device has to be provisioned. 0x434E is the project's own DIY code, not CSA-assigned (see `MANUFACTURER_ID.md`).

## 5. Connect the board

1. Unplug the HVAC terminal block and take the board out of the case.
2. Plug a **data-capable** USB-C cable into the board's USB-C port. USB powers the board.
3. Check that the port is there:
   ```sh
   ls -l /dev/ttyACM*        # expect /dev/ttyACM0, group uucp
   lsusb | grep -i 303a      # Espressif USB JTAG/serial debug unit
   ```

A blank chip (or one running working firmware) enumerates on its own over the native USB, so you normally **don't** need download mode. If no port appears (bad firmware or USB disabled), enter the ROM bootloader by hand with the board's buttons: **hold BOOT, tap RESET, release BOOT**, then flash again. (The schematic has BOOT on IO9 and RESET on EN. In the finished case, RESET is the pinhole.)

The STWD100 external watchdog stays disarmed while flashing and in the bootloader: its ~EN pull-up keeps it off until the firmware drives IO11 low. So it won't reset the chip mid-flash.

## 6. First flash

For a brand-new chip, or a board that has run other firmware, erase it first. That clears NVS, otadata and `zb_storage`, so you start with default settings and no stale Zigbee network:

```sh
idf.py -p /dev/ttyACM0 erase-flash
idf.py -p /dev/ttyACM0 flash monitor
```

Exit the monitor with `Ctrl+]`. Later re-flashes on the bench only need `flash monitor`. Skip the erase if you want to keep the settings and the pairing.

**A healthy first boot.** The firmware has not run on real hardware, so the IDF banner around these lines is unverified. What the sources do print:

- Outputs stay off (`EVT out W=0 Y=0 G=0`). The firmware **boots in failsafe** and leaves it after 3 consecutive good SHT40 readings. The sensor is read every 2 s. A good reading is not logged. A bad one logs `sht40: CRC error` or `sht40: I2C error`. Init failure logs `EVT fault sensor init (control stays in failsafe)`.
- The e-paper draws the home screen: temperature in °F, mode, and status bar. Until the coordinator's time is accepted, it shows **"Day (no clock)"** with no HH:MM digits. The first time read is 5 s after the join. Once that read is accepted, a 24-hour **HH:MM** is drawn on the next minute refresh and then every minute.
- The bottom line shows **"Zigbee: pairing..."**. A factory-new device starts network steering on its own (`factory new: start network steering (pairing)`). A failed attempt backs off: 10 s, 30 s, 1 min, 2 min, 5 min, then every 15 min (at most 14 attempts in 2 h).
- About 10 s after boot, once the loop has run cleanly, the log says `arming STWD100`.

## 7. Bring-up checklist (still on USB, no HVAC)

From `firmware/README.md`. Do this before connecting 24 VAC:

1. The screen draws a home screen with a temperature. A good SHT40 read is not logged (the sensor is read every 2 s); CRC/I2C errors and `EVT fault sensor init` are. If the image is mirrored or upside down, rebuild with `idf.py -DEPD_ROTATION=3 build` and re-flash.
2. W/Y/G stay low at boot, in the bootloader, and during and after a forced watchdog reset (check with a scope, or an LED on DRV_*).
3. The STWD100 resets the chip when the loop stalls (for example, a debug build with a `while(1)`).
4. Self-heating: after 2 h in the case, compare against a reference thermometer, then set `CONFIG_THERMO_SELF_HEAT_OFFSET_C100`.
5. Only then wire R/C/W/Y/G, and watch the equipment during the first heat and cool calls.

## 8. Pair with Home Assistant (ZHA)

**Install the quirk first** (`firmware/ha/zha_quirk/`):

1. In `configuration.yaml`:
   ```yaml
   zha:
     custom_quirks_path: /config/custom_zha_quirks/
   ```
2. Copy `firmware/ha/zha_quirk/connie_thermostat_v3.py` into `/config/custom_zha_quirks/`.
3. Restart Home Assistant.

**Pair:**

1. In HA, go to Settings → Devices & services → Zigbee Home Automation → **Add device**. This opens joining.
2. A freshly erased thermostat is already steering ("Zigbee: pairing..."), so it should join on its own. To start pairing by hand, on the main screen **hold M for 5 s** until the bottom bar says `Let go: pair  |  hold on: reset`, then **release between 5 and 8 s**. You can also use Menu → *Zigbee* → hold M for 1.5 s (the row reads "Hold M: pair").
3. The device page should list the quirk as "(Connie / thermostat-v3)". You get a climate entity with off/heat/cool, fan auto/on, temperature and humidity sensors, plus the boost and schedule entities. If it paired before you installed the quirk, use Device → ⋮ → **Reconfigure**, or remove it and pair again.
4. Close joining when you're done. Join uses an open install-code policy (`install_code_policy` false): the well-known Zigbee 3.0 trust-center link key.

**Lovelace card** (optional, `firmware/ha/lovelace-card/`):

- *Manual:* copy `dist/connie-thermostat-card.js` to `/config/www/connie-thermostat-card.js`. Then go to Settings → Dashboards → ⋮ → Resources → Add resource, URL `/local/connie-thermostat-card.js`, type *JavaScript module*. Reload the browser, then edit the dashboard → Add card → **Connie Thermostat V3**.
- *HACS:* HACS → ⋮ → Custom repositories → `https://github.com/Connicpu/thermostat`, type *Dashboard*. HACS installs `connie-thermostat-card.js` from the **GitHub release asset**, not from the `firmware/ha/lovelace-card/` subfolder. A release that does not attach that file cannot be installed this way; use the manual copy instead.

(Zigbee2MQTT is also supported through the untested external converter at `firmware/z2m/thermostat-v3.js`. See the firmware README.)

After the board is on the wall, update it **over Zigbee OTA**, not USB (README section *OTA updates over Zigbee*).

## 9. Troubleshooting

| Symptom | What to try |
|---|---|
| **No `/dev/ttyACM0`** | Try another cable (many are charge-only) and another port. Run `dmesg -w` while you plug in. Enter download mode with BOOT + RESET (step 5). |
| **Permission denied on the port** | Check you're in `uucp` (`groups`), then log out and in again. `ls -l /dev/ttyACM0` should show group `uucp`. |
| **Port disappears during flash or monitor** | That's normal for USB-Serial-JTAG on reset: the device re-enumerates. If the port number changes (ttyACM1), pass the new one. |
| **Stuck in download mode** (log shows `waiting for download`) | Tap RESET without holding BOOT. If it keeps happening, the IO9 strap (BOOT) is being held low. Check the button. GPIO8 (LED) is also a strap and must be high at reset. |
| **Brownout resets / reboots during radio TX** | Use a better cable or port, or a powered hub. Never "fix" it by adding 24 VAC while USB is connected. |
| **Display blank** | Check the 24-pin FPC is fully seated and latched. A stuck BUSY line doesn't stop control and won't reboot the board, so the log keeps running: look there for display errors. Mirrored or upside down: `idf.py -DEPD_ROTATION=3 build`. |
| **"SAFE MODE: repeated resets" banner** | 5 boots in a row that ended before 3 min of healthy uptime (power cycles count). The board retries after 30 min. A factory reset doesn't clear this counter. Find out why it's rebooting first (watch the monitor log). |
| **Won't join Zigbee** | Make sure ZHA joining is open and the board is close to the coordinator or a router. Retries back off (10 s, 30 s, 1 min, 2 min, 5 min, then every 15 min; ≤ 14 attempts in 2 h), so start pairing again with M (hold 5 s, release between 5 and 8 s). If it was paired somewhere else before, factory-reset it. On the bench, `idf.py erase-flash` also wipes `zb_storage`. |
| **Factory reset** | On the main screen, hold **M for 15 s**. At 8 s a full-screen warning appears (`FACTORY RESET` / "Keep holding M to reset" / "Release to cancel"). Keep holding. This turns outputs off, erases settings, leaves the network and reboots, and the board then starts pairing on its own. Releasing between 8 and 15 s cancels. |

## 10. Optional: run the tests locally

These only need host tools, no board:

```sh
sudo pacman -S --needed gcc cmake ninja python
cd thermostat/firmware

# Host unit tests (ASan/UBSan, -Werror; no ESP-IDF needed)
CMAKE_GENERATOR=Ninja tools/run_host_tests.sh

# HOOTL simulated-hardware scenarios (needs the esp-zigbee headers once)
. ~/esp/esp-idf/export.sh && idf.py set-target esp32c6 reconfigure
tools/run_hootl.sh                    # ~25 s, parallel
python3 test/hootl/hootl.py --list    # what each scenario asserts

# ZHA quirk tests (CI uses Python 3.13 with these pins)
cd ha/zha_quirk
python -m venv .venv && .venv/bin/pip install pytest==9.1.1 zha==2.3.1 zha-quirks==2.3.0 zigpy==2.3.0
.venv/bin/python -m pytest -q
```
