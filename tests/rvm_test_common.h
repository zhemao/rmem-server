#ifndef __RVM_TEST_COMMON__
#define __RVM_TEST_COMMON__

rvm_cfg_t* initialize_rvm(char* host, char* port,
        bool recovery, create_rmem_layer_f create_layer) {
    rvm_opt_t opt;
    opt.host = host;
    opt.port = port;
//    opt.alloc_fp = simple_malloc;
//    opt.free_fp = simple_free;
    opt.alloc_fp = buddy_malloc;
    opt.free_fp = buddy_free;
    opt.nentries = DEFAULT_BLK_TBL_NENT;
    opt.recovery = recovery;

    LOG(8, ("rvm_cfg_create\n"));
    rvm_cfg_t *cfg = rvm_cfg_create(&opt, create_layer);
    CHECK_ERROR(cfg == NULL, 
            ("FAILURE: Failed to initialize rvm configuration - %s\n", strerror(errno)));

    return cfg;
}

#endif
