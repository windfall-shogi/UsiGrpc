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
    ponder_move = 0

    with grpc.insecure_channel('localhost:50051') as channel:
        stub = usi_pb2_grpc.UsiProtocolStub(channel)

        empty = Empty()
        messages = stub.RecieveMessage(empty)
        for msg in messages:
            print(msg, end='')
            if msg.WhichOneof('msg') == 'sm':
                if msg.sm == usi_pb2.USI:
                    engine_id = usi_pb2.EngineId(name='foo', author='bar')
                    stub.SendEngineId(engine_id)

                    options = usi_pb2.EngineOption(options=[
                        usi_pb2.Option(index=0, name='USI_Ponder', check=usi_pb2.Option.Check(value=True)),
                        usi_pb2.Option(index=1, name='USI_Hash', spin=usi_pb2.Option.Spin(value=5, min_value=0, max_value=5)),
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
                    ponder_move = best_move(board, stub)
                elif msg.sm == usi_pb2.NEWGAME:
                    pass
            elif msg.WhichOneof('msg') == 'option':
                pass
            elif msg.WhichOneof('msg') == 'go':
                sfen = msg.go.position.start + ' moves'
                board.set_sfen(sfen)
                print(board)
                for m in msg.go.position.moves:
                    board.push_psv(m & 0xFFFF)
                print(board)

                if msg.go.ponder:
                    continue
                elif msg.go.mate:
                    # 詰み探索
                    continue
                
                if msg.go.hit:
                    board.push_psv(ponder_move)
                if msg.go.WhichOneof('time') != 'infinite':
                    ponder_move = best_move(board, stub)

            elif msg.WhichOneof('msg') == 'gameover':
                pass


def best_move(board, stub):
    move = get_move(board, push=True)
    if move != (2 << 7) + 2:
        ponder = get_move(board, push=False)
    else:
        ponder = (2 << 7) + 2
    m = usi_pb2.BestMove(move=move, ponder=ponder)
    stub.SendBestMove(m)

    return ponder


def get_move(board, push):
    legal_moves = [m for m in board.legal_moves]
    print(len(legal_moves))
    if len(legal_moves) == 0:
        move = (2 << 7) + 2    # resign
    else:
        move = cshogi.move16_to_psv(legal_moves[0] & 0xFFFF)
        if cshogi.move_is_drop(legal_moves[0]):
            tmp = (move >> 7) & 0x3F
            turn = board.turn
            tmp |= turn << 4
            move |= tmp << 16
        else:
            piece = board.piece(cshogi.move_from(legal_moves[0]))
            promotion = cshogi.move_is_promotion(legal_moves[0])
            piece |= promotion << 3
            move |= piece << 16
        if push:
            board.push(legal_moves[0])
    return move


def main():
    message_loop()    
    pass


if __name__ == '__main__':
    main()
