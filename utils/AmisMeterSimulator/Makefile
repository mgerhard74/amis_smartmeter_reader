OPTIONS := --port=/dev/ttyUSB1 \
           --remote-url=http://YOUR-AMIS-IPNUMBER/rest \
           -v \
           # --remote-username=admin --remote-password=admin \

# overrule "OPTIONS" (only if 'local_config.mk' exists)
# for privacy: config in extra file
-include local_config.mk


run-test:
	python3 ./AmisMeterSimulator.py $(OPTIONS)
	# --key=1234567890abcdef1234567890abcde1

run-test-no-serial:
	python3 ./AmisMeterSimulator.py $(OPTIONS) "--serialnumber="
