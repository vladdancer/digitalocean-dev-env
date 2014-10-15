#!/usr/bin/env bash
#
#

sh ./ansible.sh
cd ./provision && ansible-playbook -i 127.0.0.1, -c local main.yml -vvv
