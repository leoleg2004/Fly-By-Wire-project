##############################
###       Domain ID        ###
##############################
export ROS_DOMAIN_ID=111 #88
export STAR_DOMAIN_ID=111 #88


##############################
###  Environment variables ###
##############################
# Resolve SERL_HOME to the parent of this script (dds/), so that
# $(SERL_HOME)/DDS/bin/... correctly resolves to dds/DDS/bin/...
SERL_HOME="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export SERL_HOME



###############################
###    STAR Libraries    ####
###############################
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SERL_HOME/DDS/bin/runtime/lib





