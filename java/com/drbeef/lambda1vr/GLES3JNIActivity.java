
package com.drbeef.lambda1vr;


import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.KeyEvent;

import static android.system.Os.setenv;


@SuppressLint("SdCardPath") public class GLES3JNIActivity extends Activity implements SurfaceHolder.Callback
{
	// Load the gles3jni library right away to make sure JNI_OnLoad() gets called as the very first thing.
	static
	{
		System.loadLibrary( "xash" );
	}

	private static final String TAG = "Lambda1VR";

	private int permissionCount = 0;
	private static final int READ_EXTERNAL_STORAGE_PERMISSION_ID = 1;
	private static final int WRITE_EXTERNAL_STORAGE_PERMISSION_ID = 2;

	String commandLineParams;

	private SurfaceView mView;
	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;
	
	@Override protected void onCreate( Bundle icicle )
	{
		Log.v( TAG, "----------------------------------------------------------------" );
		Log.v( TAG, "GLES3JNIActivity::onCreate()" );
		super.onCreate( icicle );

		mView = new SurfaceView( this );
		setContentView( mView );
		mView.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes( params );



		checkPermissionsAndInitialize();
	}

	/** Initializes the Activity only if the permission has been granted. */
	private void checkPermissionsAndInitialize() {
		// Boilerplate for checking runtime permissions in Android.
		if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			ActivityCompat.requestPermissions(
					GLES3JNIActivity.this,
					new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE},
					WRITE_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			permissionCount++;
		}

		if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED)
		{
			ActivityCompat.requestPermissions(
					GLES3JNIActivity.this,
					new String[] {Manifest.permission.READ_EXTERNAL_STORAGE},
					READ_EXTERNAL_STORAGE_PERMISSION_ID);
		}
		else
		{
			permissionCount++;
		}

		if (permissionCount == 2) {
			// Permissions have already been granted.
			create();
		}
	}

	/** Handles the user accepting the permission. */
	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
		if (requestCode == READ_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
				permissionCount++;
			}
			else
			{
				System.exit(0);
			}
		}

		if (requestCode == WRITE_EXTERNAL_STORAGE_PERMISSION_ID) {
			if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
				permissionCount++;
			}
			else
			{
				System.exit(0);
			}
		}

		checkPermissionsAndInitialize();
	}

	public void create()
	{
		copy_asset(getFilesDir().getPath(), "extras.pak", false);
		copy_asset("/sdcard/xash/valve/", "config.cfg", false); // Copy in case user has deleted their config
		copy_asset("/sdcard/xash/", "commandline.txt", false); // Copy in case user has deleted their config

		//Copy modified weapon models
		if (!(new File("/sdcard/xash/valve/models/no_copy").exists()))
		{
			copy_asset("/sdcard/xash/valve/", "models/v_9mmar.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_9mmhandgun.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_crowbar.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_357.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_crossbow.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_egon.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_gauss.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_grenade.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_hgun.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_rpg.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_satchel.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_satchel_radio.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_shotgun.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_squeak.mdl", true);
			copy_asset("/sdcard/xash/valve/", "models/v_tripmine.mdl", true);
		}

		//Set default environment
		try {
			setenv("XASH3D_BASEDIR", "/sdcard/xash/", true);
			setenv("XASH3D_ENGLIBDIR", getFilesDir().getParentFile().getPath() + "/lib", true);
			setenv("XASH3D_GAMELIBDIR", getFilesDir().getParentFile().getPath() + "/lib", true);
			setenv("XASH3D_GAMEDIR", "valve", true);
			setenv( "XASH3D_EXTRAS_PAK1", getFilesDir().getPath() + "/extras.pak", true );
		}
		catch (Exception e)
		{

		}
		

		//Read these from a file and pass through
		commandLineParams = new String("xash3d -dev 3 -log");

		//See if user is trying to use command line params
		if(new File("/sdcard/xash/commandline.txt").exists()) // should exist!
		{
			BufferedReader br;
			try {
				br = new BufferedReader(new FileReader("/sdcard/xash/commandline.txt"));
				String s;
				StringBuilder sb=new StringBuilder(0);
				while ((s=br.readLine())!=null)
					sb.append(s + " ");
				br.close();

				commandLineParams = new String(sb.toString());
			} catch (FileNotFoundException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		mNativeHandle = GLES3JNILib.onCreate( this, commandLineParams );
	}
	
	public void copy_asset(String path, String name, boolean forceOverwrite) {
		File f = new File(path + "/" + name);
		if (!f.exists() || forceOverwrite) {
			
			//Ensure we have an appropriate folder
			new File(path).mkdirs();
			_copy_asset(name, path + "/" + name);
		}
	}

	public void _copy_asset(String name_in, String name_out) {
		AssetManager assets = this.getAssets();

		try {
			InputStream in = assets.open(name_in);
			OutputStream out = new FileOutputStream(name_out);

			copy_stream(in, out);

			out.close();
			in.close();

		} catch (Exception e) {

			e.printStackTrace();
		}

	}

	public static void copy_stream(InputStream in, OutputStream out)
			throws IOException {
		byte[] buf = new byte[1024];
		while (true) {
			int count = in.read(buf);
			if (count <= 0)
				break;
			out.write(buf, 0, count);
		}
	}


	@Override protected void onStart()
	{
		Log.v( TAG, "GLES3JNIActivity::onStart()" );
		super.onStart();

		GLES3JNILib.onStart( mNativeHandle );
	}

	@Override protected void onResume()
	{
		Log.v( TAG, "GLES3JNIActivity::onResume()" );
		super.onResume();

		GLES3JNILib.onResume( mNativeHandle );
	}

	@Override protected void onPause()
	{
		Log.v( TAG, "GLES3JNIActivity::onPause()" );
		GLES3JNILib.onPause( mNativeHandle );
		super.onPause();
	}

	@Override protected void onStop()
	{
		Log.v( TAG, "GLES3JNIActivity::onStop()" );
		GLES3JNILib.onStop( mNativeHandle );
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		Log.v( TAG, "GLES3JNIActivity::onDestroy()" );

		if ( mSurfaceHolder != null )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
		}

		GLES3JNILib.onDestroy( mNativeHandle );

		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated( SurfaceHolder holder )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceCreated()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceCreated( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged( SurfaceHolder holder, int format, int width, int height )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceChanged()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceChanged( mNativeHandle, holder.getSurface() );
			mSurfaceHolder = holder;
		}
	}
	
	@Override public void surfaceDestroyed( SurfaceHolder holder )
	{
		Log.v( TAG, "GLES3JNIActivity::surfaceDestroyed()" );
		if ( mNativeHandle != 0 )
		{
			GLES3JNILib.onSurfaceDestroyed( mNativeHandle );
			mSurfaceHolder = null;
		}
	}
}
