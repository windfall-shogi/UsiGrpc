#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from threading import Thread

import cshogi
import grpc
from google.protobuf.empty_pb2 import Empty

import usi_pb2_grpc
import usi_pb2

from queue import Queue
import time


def message_loop():
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = usi_pb2_grpc.UsiProtocolStub(channel)

        empty = Empty()
        messages = stub.RecieveMessage(empty)
        for msg in messages:
            print(msg)
            if msg.WhichOneof('msg') == 'sm':
                if msg.sm == usi_pb2.USI:
                    engine_id = usi_pb2.EngineId(name='foo', author='bar')
                    stub.SendEngineId(engine_id)
                elif msg.sm == usi_pb2.QUIT:
                    break


def main():
    message_loop()    
    pass


if __name__ == '__main__':
    main()
