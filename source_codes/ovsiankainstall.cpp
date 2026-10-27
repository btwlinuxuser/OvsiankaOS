#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

std::string shellQuote(const std::string& value) {
    std::string result = "'";
    for (char c : value) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

bool run(const std::string& command) {
    std::cout << "\n> " << command << '\n';
    return std::system(command.c_str()) == 0;
}

bool validName(const std::string& value) {
    static const std::regex pattern("^[a-zA-Z0-9][a-zA-Z0-9_-]*$");
    return std::regex_match(value, pattern);
}

bool validPathValue(const std::string& value) {
    static const std::regex pattern("^[a-zA-Z0-9_./@+-]+$");
    return std::regex_match(value, pattern);
}

int main() {
    if (geteuid() != 0) {
        std::cerr << "Run this program as root.\n";
        return 1;
    }

    std::string disk;
    std::string hostname;
    std::string username;
    std::string timezone;
    std::string locale;
    std::string keymap;
    std::string confirm;

    std::cout << "Target disk (for example /dev/sda or /dev/nvme0n1): ";
    std::getline(std::cin, disk);

    if (disk.rfind("/dev/", 0) != 0 ||
        !std::regex_match(disk, std::regex("^/dev/[a-zA-Z0-9/_-]+$"))) {
        std::cerr << "Invalid disk path.\n";
        return 1;
    }

    std::cout << "Computer name [ovsianka]: ";
    std::getline(std::cin, hostname);
    if (hostname.empty()) hostname = "ovsianka";

    std::cout << "Username: ";
    std::getline(std::cin, username);

    std::cout << "Time zone [America/New_York]: ";
    std::getline(std::cin, timezone);
    if (timezone.empty()) timezone = "America/New_York";

    std::cout << "System locale [en_US.UTF-8]: ";
    std::getline(std::cin, locale);
    if (locale.empty()) locale = "en_US.UTF-8";

    std::cout << "Console keyboard layout [us]: ";
    std::getline(std::cin, keymap);
    if (keymap.empty()) keymap = "us";

    if (!validName(hostname)) {
        std::cerr << "Invalid computer name. Use letters, numbers, _ or -.\n";
        return 1;
    }

    if (!validName(username)) {
        std::cerr << "Invalid username. Use letters, numbers, _ or -.\n";
        return 1;
    }

    if (!validPathValue(timezone) ||
        !validPathValue(locale) ||
        !validPathValue(keymap)) {
        std::cerr << "Invalid time zone, locale, or keyboard layout value.\n";
        return 1;
    }

    if (locale.find(".UTF-8") == std::string::npos) {
        std::cerr << "Enter a UTF-8 locale, for example en_US.UTF-8.\n";
        return 1;
    }

    if (!run("lsblk " + shellQuote(disk))) {
        std::cerr << "Could not list the selected disk.\n";
        return 1;
    }

    std::cout << "\nWARNING: all data on " << disk << " will be erased.\n"
              << "Type ERASE to continue: ";
    std::getline(std::cin, confirm);

    if (confirm != "ERASE") {
        std::cout << "Installation cancelled.\n";
        return 0;
    }

    const bool nvme =
        disk.find("nvme") != std::string::npos ||
        disk.find("mmcblk") != std::string::npos;

    const std::string partition1 = disk + (nvme ? "p1" : "1");
    const std::string partition2 = disk + (nvme ? "p2" : "2");

    std::cout << "\nThe installer will now repartition " << disk << ".\n"
              << "Type ERASE again to confirm: ";
    std::getline(std::cin, confirm);

    if (confirm != "ERASE") {
        std::cout << "Installation cancelled.\n";
        return 0;
    }

    if (!run("parted -s " + shellQuote(disk) +
             " mklabel gpt "
             "mkpart ESP fat32 1MiB 1GiB "
             "set 1 esp on "
             "mkpart primary ext4 1GiB 100%")) {
        std::cerr << "Disk partitioning failed.\n";
        return 1;
    }

    if (!run("mkfs.fat -F32 " + shellQuote(partition1))) return 1;
    if (!run("mkfs.ext4 -F " + shellQuote(partition2))) return 1;
    if (!run("mount " + shellQuote(partition2) + " /mnt")) return 1;
    if (!run("mount --mkdir " + shellQuote(partition1) + " /mnt/boot")) return 1;

    if (!run("install -Dm644 /usr/share/ovsianka/grub.png /mnt/boot/grub/ovsianka.png")) {
        std::cerr << "Could not copy the GRUB background image.\n";
        return 1;
    }

    if (!run("pacstrap -K /mnt base linux linux-firmware networkmanager sudo alsa-utils amd-ucode arch-install-scripts archinstall b43-fwcutter base bc bcachefs-tools bind bolt brltty btrfs-progs clonezilla cloud-init cryptsetup darkhttpd ddrescue dhcpcd diffutils dmidecode dmraid dnsmasq dosfstools e2fsprogs edk2-shell efibootmgr espeakup ethtool exfatprogs f2fs-tools fatresize foot-terminfo fsarchiver ghostty-terminfo gpart gpm gptfdisk grml-zsh-config grub hdparm hyperv intel-ucode irssi iw iwd jfsutils kitty-terminfo kmscon-terminfo ldns less lftp libfido2 libusb-compat linux-firmware-marvell livecd-sounds lsscsi lvm2 lynx man-db man-pages mc mdadm memtest86+ memtest86+-efi mkinitcpio mkinitcpio-archiso mkinitcpio-nfs-utils mmc-utils modemmanager mtools nano nbd ndisc6 nfs-utils nilfs-utils nmap ntfsprogs nvme-cli open-iscsi open-vm-tools openconnect openpgp-card-tools openssh openvpn partclone parted partimage pcsclite ppp pptpclient pv qemu-guest-agent refind reflector rsync rxvt-unicode-terminfo screen sdparm sequoia-sq sg3_utils smartmontools sof-firmware squashfs-tools syslinux systemd-resolvconf tcpdump terminus-font testdisk tmux tpm2-tools tpm2-tss udftools usb_modeswitch usbmuxd usbutils vim virtualbox-guest-utils-nox vpnc wireless-regdb wireless_tools wpa_supplicant wvdial xdg-utils xfsprogs xl2tpd zsh metasploit openbsd-netcat socat masscan bettercap nginx php ruby rubygems rust wireshark-cli clamav rkhunter fail2ban lynis nftables fish xfce4 xorg-server lightdm lightdm-gtk-greeter kitty iwd")) {
        std::cerr << "Base system installation failed.\n";
        return 1;
    }

    //Внимание на эту строку!!!
    /*if (!run("install -Dm755 /usr/local/bin/ovsiankafetch /mnt/usr/local/bin/ovsiankafetch")) {
        std::cerr << "Could not install ovsiankafetch.\n";
        return 1;
    }*/

    if (!run("genfstab -U /mnt >> /mnt/etc/fstab")) {
        std::cerr << "Could not generate /etc/fstab.\n";
        return 1;
    }

    const std::string setupPath = "/mnt/root/arch-setup.sh";
    FILE* setup = std::fopen(setupPath.c_str(), "w");

    if (!setup) {
        std::cerr << "Could not create the setup script.\n";
        return 1;
    }

    std::fprintf(setup,
        "#!/bin/bash\n"
        "set -e\n"
        "ln -sf /usr/share/zoneinfo/%s /etc/localtime\n"
        "hwclock --systohc\n"
        "sed -i 's/^#\\(%s UTF-8\\)/\\1/' /etc/locale.gen\n"
        "locale-gen\n"
        "echo 'LANG=%s' > /etc/locale.conf\n"
        "echo 'KEYMAP=%s' > /etc/vconsole.conf\n"
        "echo '%s' > /etc/hostname\n"
        "useradd -m -G wheel -s /bin/bash '%s'\n"
        "sed -i 's/^#\\s*%%wheel ALL=(ALL:ALL) ALL/%%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers\n"
        "printf 'GRUB_DISTRIBUTOR=\"Ovsianka\"\\nGRUB_BACKGROUND=\"/boot/grub/ovsianka.png\"\\n' >> /etc/default/grub\n"
        "grub-install --target=x86_64-efi --efi-directory=/boot --bootloader-id=Ovsianka\n"
        "grub-mkconfig -o /boot/grub/grub.cfg\n"
        "systemctl enable NetworkManager\n",
        timezone.c_str(),
        locale.c_str(),
        locale.c_str(),
        keymap.c_str(),
        hostname.c_str(),
        username.c_str()
    );

    std::fclose(setup);
    chmod(setupPath.c_str(), 0700);

    if (!run("arch-chroot /mnt bash /root/arch-setup.sh")) {
        std::cerr << "System configuration failed.\n";
        return 1;
    }

    if (!run("arch-chroot /mnt passwd")) {
        std::cerr << "Could not set the root password.\n";
        return 1;
    }

    if (!run("arch-chroot /mnt passwd " + shellQuote(username))) {
        std::cerr << "Could not set the user password.\n";
        return 1;
    }

    if (!run("rm -f /mnt/root/arch-setup.sh")) {
        std::cerr << "Could not remove the temporary setup script.\n";
        return 1;
    }

    std::cout << "\nInstallation complete. Run:\n"
              << "  umount -R /mnt\n"
              << "  reboot\n";

    return 0;
}
//ag
