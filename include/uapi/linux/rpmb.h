/*
 * Copyright (C) 2015-2016, Intel Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _UAPI_LINUX_RPMB_H_
#define _UAPI_LINUX_RPMB_H_

#include <linux/types.h>

/**
 * struct rpmb_frame - rpmb frame as defined by specs
 *
 * @stuff        : stuff bytes
 * @key_mac      : The authentication key or the message authentication
 *                 code (MAC) depending on the request/response type.
 *                 The MAC will be delivered in the last (or the only)
 *                 block of data.
 * @data         : Data to be written or read by signed access.
 * @nonce        : Random number generated by the host for the requests
 *                 and copied to the response by the RPMB engine.
 * @write_counter: Counter value for the total amount of the successful
 *                 authenticated data write requests made by the host.
 * @addr         : Address of the data to be programmed to or read
 *                 from the RPMB. Address is the serial number of
 *                 the accessed block (half sector 256B).
 * @block_count  : Number of blocks (half sectors, 256B) requested to be
 *                 read/programmed.
 * @result       : Includes information about the status of the write counter
 *                 (valid, expired) and result of the access made to the RPMB.
 * @req_resp     : Defines the type of request and response to/from the memory.
 */
struct rpmb_frame {
	__u8   stuff[196];
	__u8   key_mac[32];
	__u8   data[256];
	__u8   nonce[16];
	__be32 write_counter;
	__be16 addr;
	__be16 block_count;
	__be16 result;
	__be16 req_resp;
} __attribute__((packed));

#define RPMB_PROGRAM_KEY       0x1    /* Program RPMB Authentication Key */
#define RPMB_GET_WRITE_COUNTER 0x2    /* Read RPMB write counter */
#define RPMB_WRITE_DATA        0x3    /* Write data to RPMB partition */
#define RPMB_READ_DATA         0x4    /* Read data from RPMB partition */
#define RPMB_RESULT_READ       0x5    /* Read result request  (Internal) */

#define RPMB_REQ2RESP(_OP) ((_OP) << 8)
#define RPMB_RESP2REQ(_OP) ((_OP) >> 8)

/* length of the part of the frame used for HMAC computation */
#define hmac_data_len \
	(sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data))

/**
 * enum rpmb_op_result - rpmb operation results
 *
 * @RPMB_ERR_OK:       operation successful
 * @RPMB_ERR_GENERAL:  general failure
 * @RPMB_ERR_AUTH:     mac doesn't match or ac calculation failure
 * @RPMB_ERR_COUNTER:  counter doesn't match or counter increment failure
 * @RPMB_ERR_ADDRESS:  address out of range or wrong address alignment
 * @RPMB_ERR_WRITE:    data, counter, or result write failure
 * @RPMB_ERR_READ:     data, counter, or result read failure
 * @RPMB_ERR_NO_KEY:   authentication key not yet programmed
 *
 * @RPMB_ERR_COUNTER_EXPIRED:  counter expired
 */
enum rpmb_op_result {
	RPMB_ERR_OK      = 0x0000,
	RPMB_ERR_GENERAL = 0x0001,
	RPMB_ERR_AUTH    = 0x0002,
	RPMB_ERR_COUNTER = 0x0003,
	RPMB_ERR_ADDRESS = 0x0004,
	RPMB_ERR_WRITE   = 0x0005,
	RPMB_ERR_READ    = 0x0006,
	RPMB_ERR_NO_KEY  = 0x0007,

	RPMB_ERR_COUNTER_EXPIRED = 0x0080
};

#define RPMB_F_WRITE     (1UL << 0)
#define RPMB_F_REL_WRITE (1UL << 1)

/**
 * struct rpmb_cmd - rpmb access command
 *
 * @flags: command flags
 *      0 - read command
 *      1 - write commnad RPMB_F_WRITE
 *      2 - reliable write RPMB_F_REL_WRITE
 * @nframes: number of rpmb frames in the command
 * @frames_ptr:  a pointer to the list of rpmb frames
 */
struct rpmb_ioc_cmd {
	__u32 flags;
	__u32 nframes;
	__aligned_u64 frames_ptr;
};

#define rpmb_ioc_cmd_set_frames(_cmd, _ptr) \
	(_cmd).frames_ptr = (__aligned_u64)(intptr_t)(_ptr)

#define rpmb_ioc_cmd_set(_cmd, _flags, _ptr, _n) do {        \
	struct rpmb_ioc_cmd *icmd = &(_cmd);                 \
	icmd->flags = (_flags);                              \
	icmd->nframes = (_n);                                \
	icmd->frames_ptr = (__aligned_u64)(intptr_t)(_ptr);  \
} while (0)

/**
 * struct rpmb_ioc_req_cmd - rpmb operation request command
 *
 * @req_type: request type:  must match the in frame req_resp
 *            program key
 *            get write counter
 *            write data
 *            read data
 * @icmd: input command
 * @ocmd: output/result command
 */
struct rpmb_ioc_req_cmd {
	__u64 req_type;
	struct rpmb_ioc_cmd icmd;
	struct rpmb_ioc_cmd ocmd;
};

/**
 * struct rpmb_ioc_seq_cmd - rpmb command sequence
 *
 * @num_of_cmds: number of commands
 * @cmds: list of rpmb commands
 */
struct rpmb_ioc_seq_cmd {
	__u64 num_of_cmds;
	struct rpmb_ioc_cmd cmds[0];
};

#define RPMB_IOC_REQ_CMD _IOWR(0xB5, 0, struct rpmb_ioc_req_cmd)
#define RPMB_IOC_SEQ_CMD _IOWR(0xB5, 1, struct rpmb_ioc_seq_cmd)

#endif /* _UAPI_LINUX_RPMB_H_ */
