#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from threading import Thread

import cshogi
import grpc

import usi_pb2_grpc
import usi_pb2

from queue import Queue
import time


def worker(q):
    while True:
        result = q.get()
        q.task_done()
        time.sleep(1)
        print(result)
        #q.task_done()


def message_loop():
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = usi_pb2_grpc.UsiProtocolStub(channel)

        response = stub.Sample(usi_pb2.EngineId(name='foo', author='bar'))
        print(response.result)

        # コネクションを確立するためにサーバー側で無視されるメッセージを送る
        #em = usi_pb2.EngineMessage(sm=usi_pb2.USI)
        em = [usi_pb2.EngineMessage(id=usi_pb2.EngineId(name='foo', author='bar'))]
        while True:
            gm = stub.Communicate(iter(em))
            #if gm.WhichOneof('msg') == 'sm':
            #    if gm.sm == usi_pb2.USI:
            #        em = usi_pb2.EngineId(name='foo', author='bar')
            #    elif gm.sm == usi_pb2.QUIT:
            #        break
            #    pass


def main():
    message_loop()

    
    pass


if __name__ == '__main__':
    main()
