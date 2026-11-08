package com.novaengine.app.engine

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * JVM unit tests for the pure-Kotlin engine bridge parsers ([NovaParsers]).
 * These validate the data contract between the native engine (libnova.so) and
 * the Compose UI without requiring the Android NDK build or an emulator.
 */
class NovaParsersTest {

    @Test
    fun parseNodeInfo_fullLine_parsesAllFields() {
        val line = "7\tPlayer\tSprite2D\t#ff00ff\t12.5,3.25\t90\t2,0.5"
        val info = NovaParsers.parseNodeInfo(line)!!
        assertEquals(7, info.id)
        assertEquals("Player", info.name)
        assertEquals("Sprite2D", info.type)
        assertEquals("#ff00ff", info.color)
        assertEquals(12.5f, info.posX)
        assertEquals(3.25f, info.posY)
        assertEquals(90f, info.rot)
        assertEquals(2f, info.scaleX)
        assertEquals(0.5f, info.scaleY)
    }

    @Test
    fun parseNodeInfo_tooShort_returnsNull() {
        assertNull(NovaParsers.parseNodeInfo("1\tname"))
        assertNull(NovaParsers.parseNodeInfo(""))
    }

    @Test
    fun parseNodeInfo_missingScaleDefaultsTo1() {
        val info = NovaParsers.parseNodeInfo("1\tName\tNode\tred\t0,0\t0\t")!!
        assertEquals(1f, info.scaleX)
        assertEquals(1f, info.scaleY)
    }

    @Test
    fun parseNodeInfo_nonNumericFieldsDoNotCrash() {
        val info = NovaParsers.parseNodeInfo("abc\tX\tNode\t\t,\t\t,")!!
        assertEquals(0, info.id)
        assertEquals(1f, info.scaleX)
        assertEquals(1f, info.scaleY)
    }

    @Test
    fun parseTree_parsesRowsAndSkipsBlankLines() {
        val dump = "1\tRoot\tNode\t0\n2\tChild\tSprite2D\t1\n\n3\tLeaf\tNode\t2\n"
        val rows = NovaParsers.parseTree(dump)
        assertEquals(3, rows.size)
        assertEquals("Root", rows[0].name)
        assertEquals(0, rows[0].depth)
        assertEquals("Leaf", rows[2].name)
        assertEquals(2, rows[2].depth)
    }

    @Test
    fun parseTree_shortLineSkipped() {
        assertTrue(NovaParsers.parseTree("1\tbroken").isEmpty())
        assertTrue(NovaParsers.parseTree("").isEmpty())
    }

    @Test
    fun parseStats_unpacksAllFields() {
        // Mirrors the bit-packing done in jni_bridge.cpp nativeStats().
        val packed = ((1L and 0xFFFF)
            or ((5L and 0xFFFF) shl 16)
            or ((7L and 0xFFFF) shl 32)
            or ((2L and 0xFFFF) shl 48))
        val stats = NovaParsers.parseStats(packed)
        assertEquals(1, stats.fps)
        assertEquals(5, stats.physicsBodies)
        assertEquals(7, stats.drawCalls)
        assertEquals(2, stats.scriptErrors)
    }

    @Test
    fun parseStats_roundTripFromBridgeLayout() {
        // fps | physicsBodies << 16 | drawCalls << 32 | scriptErrors << 48
        val packed = (60L) or (12L shl 16) or (42L shl 32) or (0L shl 48)
        val stats = NovaParsers.parseStats(packed)
        assertEquals(60, stats.fps)
        assertEquals(12, stats.physicsBodies)
        assertEquals(42, stats.drawCalls)
        assertEquals(0, stats.scriptErrors)
    }
}
