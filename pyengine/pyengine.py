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
    board = cshogi.Board()

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

                    options = usi_pb2.EngineOption(options=[
                        usi_pb2.Option(index=0, name='Check0', check=usi_pb2.Option.Check(value=True)),
                        usi_pb2.Option(index=1, name='USI_HASH', spin=usi_pb2.Option.Spin(value=5, min_value=0, max_value=5)),
                        usi_pb2.Option(index=2, name='Combo2', combo=usi_pb2.Option.Combo(value='a', candidates=['a', 'b', 'c'])),
                        usi_pb2.Option(index=3, name='Button3', button=usi_pb2.Option.Button()),
                        usi_pb2.Option(index=4, name='String4', str=usi_pb2.Option.String(value='abc')),
                        usi_pb2.Option(index=5, name='Filename5', filename=usi_pb2.Option.Filename(value='def'))
                    ])
                    stub.SendOption(options)

                    stub.SendUsiOk(empty)
                elif msg.sm == usi_pb2.QUIT:
                    break
                elif msg.sm == usi_pb2.ISREADY:
                    stub.SendReady(empty)
                elif msg.sm == usi_pb2.STOP:
                    move = list(board.legal_moves)[0]
                    best_move = cshogi.move_to_psv(move & 0xFF)
                    board.push(move)
                    move = list(board.legal_moves)[0]
                    ponder = cshogi.move_to_psv(move & 0xFF)
                    m = usi_pb2.BestMove(move=best_move, ponder=ponder)
                    stub.SendBestMove(m)
                elif msg.sm == usi_pb2.NEWGAME:
                    pass

def main():
    message_loop()    
    pass


if __name__ == '__main__':
    main()
