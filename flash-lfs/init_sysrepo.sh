#!/bin/bash

set -euo pipefail

# main input folder
SYSREPO_DIR="sysrepo"

# main output folder
OUTPUT_FOLDER="input"

# sysrepo environment variables
SR_REPO_PATH="${OUTPUT_FOLDER}/etc/sysrepo"
SR_SHM_DIR=$(mktemp -d)

# transport selection: "ssh" or "tls"
TRANSPORT="ssh"

# startup data files to import (applied in order)
SR_STARTUP_CONFIG_FILES=(
    "$SYSREPO_DIR/startup/nacm.json"
    "$SYSREPO_DIR/startup/netconf-server-${TRANSPORT}.json"
)

# executables
SYSREPOCTL="sysrepoctl"
SYSREPOCFG="sysrepocfg"

# YANG module searchpath
YANG_DIR="$SYSREPO_DIR/yang"

# list of YANG modules to install with their features
YANG_MODULES=(
"iana-ssh-encryption-algs@2024-10-16.yang"
"iana-ssh-key-exchange-algs@2024-10-16.yang"
"iana-ssh-mac-algs@2024-10-16.yang"
"iana-ssh-public-key-algs@2024-10-16.yang"
"iana-tls-cipher-suite-algs@2024-10-16.yang"
"ietf-x509-cert-to-name@2014-12-10.yang"
"iana-crypt-hash@2014-04-04.yang -e crypt-hash-md5 -e crypt-hash-sha-256 -e crypt-hash-sha-512"
"ietf-crypto-types@2024-10-10.yang -e cleartext-passwords -e cleartext-private-keys"
"ietf-keystore@2024-10-10.yang -e central-keystore-supported -e inline-definitions-supported -e asymmetric-keys"
"ietf-truststore@2024-10-10.yang -e central-truststore-supported -e inline-definitions-supported -e certificates -e public-keys"
"ietf-tcp-common@2024-10-10.yang -e keepalives-supported"
"ietf-tcp-server@2024-10-10.yang -e tcp-server-keepalives"
"ietf-tcp-client@2024-10-10.yang -e local-binding-supported -e tcp-client-keepalives"
"ietf-ssh-common@2024-10-10.yang -e algorithm-discovery -e transport-params"
"ietf-ssh-server@2024-10-10.yang -e local-users-supported -e local-user-auth-publickey -e local-user-auth-password -e local-user-auth-none"
"ietf-tls-common@2024-10-10.yang -e algorithm-discovery -e tls12 -e tls13 -e hello-params"
"ietf-tls-server@2024-10-10.yang -e server-ident-x509-cert -e client-auth-supported -e client-auth-x509-cert"
"ietf-netconf-server@2025-04-24.yang -e ssh-listen -e tls-listen -e ssh-call-home -e tls-call-home -e central-netconf-server-supported"
"libnetconf2-netconf-server@2026-04-17.yang"

"ietf-interfaces@2018-02-20.yang"
"ietf-ip@2018-02-22.yang"
"ietf-netconf@2013-09-29.yang -e writable-running -e candidate -e rollback-on-error -e validate -e startup -e url -e xpath -e confirmed-commit"
"ietf-netconf-nmda@2019-01-07.yang -e origin -e with-defaults"
"notifications@2008-07-14.yang"
"nc-notifications@2008-07-14.yang"
"ietf-netconf-monitoring@2010-10-04.yang"
"ietf-network-instance@2019-01-21.yang"
"ietf-subscribed-notifications@2019-09-09.yang -e encode-xml -e replay -e subtree -e xpath"
"ietf-yang-push@2019-09-09.yang -e on-change"
"netopeer-notifications@2026-01-05.yang"
"ietf-system-capabilities@2022-02-17.yang"
"ietf-notification-capabilities@2022-02-17.yang"
"ietf-netconf-private-candidate@2026-02-03.yang -e private-candidate"
)

#
# preflight checks
#

echo "--- Checking required programs ---"
REQUIRED_PROGRAMS=("$SYSREPOCTL" "$SYSREPOCFG" "jq")
missing=0
for prog in "${REQUIRED_PROGRAMS[@]}"; do
    if ! command -v "$prog" &>/dev/null; then
        echo "ERROR: Required program not found: '$prog'" >&2
        missing=1
    else
        echo "  Found: $prog ($(command -v "$prog"))"
    fi
done
if [ $missing -ne 0 ]; then
    echo "ERROR: One or more required programs are missing. Aborting." >&2
    exit 1
fi
echo "  All required programs found."

#
# cleanup
#
echo ""
echo "--- Cleaning up previous sysrepo repository ---"
echo "  Removing: ${SR_REPO_PATH}"
rm -rf ${SR_REPO_PATH}
echo "  Cleanup done."
echo "  Using sysrepo SHM dir: ${SR_SHM_DIR}"

#
# execution
#

function INSTALL_MODULE_CMD() {
    if [ -z "${CMD_INSTALL}" ]; then
        CMD_INSTALL="'$SYSREPOCTL' -s '$1' -v2"
    fi

    CMD_INSTALL="$CMD_INSTALL -i $1/$2"
}

function SETUP_CMD() {
    module_dir="$1"     # first argument - module directory
    shift               # shift all args to the left
    modules=("$@")      # the rest of the arguments are module names (and their features)
    for i in "${modules[@]}"; do
        # prepare command to install module with all its features
        INSTALL_MODULE_CMD "$module_dir" "$i"
    done
}

echo ""
echo "--- Setting up sysrepo environment ---"
echo "  SYSREPO_REPOSITORY_PATH=${SR_REPO_PATH}"
echo "  SYSREPO_SHM_DIR=${SR_SHM_DIR}"
echo "  TRANSPORT=${TRANSPORT}"

if [[ "$TRANSPORT" != "ssh" && "$TRANSPORT" != "tls" ]]; then
    echo "ERROR: Invalid TRANSPORT value '${TRANSPORT}'. Must be 'ssh' or 'tls'." >&2
    exit 1
fi

# set up environment
export SYSREPO_REPOSITORY_PATH=${SR_REPO_PATH} SYSREPO_SHM_DIR=${SR_SHM_DIR}

# the install command will be stored in this variable
CMD_INSTALL=

echo ""
echo "--- Preparing install command for ${#YANG_MODULES[@]} YANG module(s) ---"

# setup the cmd for install
SETUP_CMD "$YANG_DIR" "${YANG_MODULES[@]}"

echo "  Install command prepared."
echo ""
echo "--- Installing YANG modules ---"

# install all the new modules
eval $CMD_INSTALL
rc=$?
if [ $rc -ne 0 ]; then
    echo "ERROR: Failed to install YANG modules (exit code $rc)." >&2
    exit $rc
fi

echo "  YANG modules installed successfully."
echo ""
echo "--- Importing startup configuration (${#SR_STARTUP_CONFIG_FILES[@]} file(s)) ---"

# merge all config files into a single temporary file and import once
SR_STARTUP_CONFIG_MERGED=$(mktemp --suffix=.json)
echo "  Merging config files into: $SR_STARTUP_CONFIG_MERGED"
jq -s 'add' "${SR_STARTUP_CONFIG_FILES[@]}" > "$SR_STARTUP_CONFIG_MERGED"
rc=$?
if [ $rc -ne 0 ]; then
    echo "ERROR: Failed to merge startup config files (exit code $rc)." >&2
    rm -f "$SR_STARTUP_CONFIG_MERGED"
    exit $rc
fi

${SYSREPOCFG} -d startup -I "$SR_STARTUP_CONFIG_MERGED" -v2
rc=$?
rm -f "$SR_STARTUP_CONFIG_MERGED"
if [ $rc -ne 0 ]; then
    echo "ERROR: Failed to import startup configuration (exit code $rc)." >&2
    exit $rc
fi

echo ""
echo "--- Cleaning up '${SR_SHM_DIR}' folder ---"
rm -rf ${SR_SHM_DIR}
echo ""

echo "  Startup configuration imported successfully."
echo ""
echo "--- sysrepo initialization complete ---"
