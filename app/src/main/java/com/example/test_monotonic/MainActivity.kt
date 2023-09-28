package com.example.test_monotonic

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.example.test_monotonic.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = stringFromJNI()
        val folder = getFilesDir().getPath()
        startTester(folder)
    }

    /**
     * A native method that is implemented by the 'test_monotonic' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String
    external fun startTester(folder: String)
    external fun stopTester()

    companion object {
        // Used to load the 'test_monotonic' library on application startup.
        init {
            System.loadLibrary("test_monotonic")
        }
    }
}